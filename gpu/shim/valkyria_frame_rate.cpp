// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) at 60 updates a second (valkyria_frame_rate.h). Every site
// was verified in the v1.00 eboot; each write carries the original bytes it replaces and
// nothing is written unless all of them match.
//
// The game's clock (FUN_003d47f0, once an update) counts the display refreshes ("ticks",
// 1/59.94 s) since the last update, never fewer than the minimum in its timing state
// ([0xa12be8], 2 as shipped: 30 updates a second), and publishes the count as an integer
// (0xa261e4), as a float (0xa261e8, the frame unit) and scaled by the game speed as the step
// (0xa12fc0). Most of the game moves by the step and is right at any rate. With the minimum
// at 1 the rest falls into a few families, each handled the same way everywhere it occurs:
//   - "one authored frame is `ticks` ticks" conversions: an authored frame is two ticks;
//   - per-update advances written as step / frame unit: the frame unit stays 2;
//   - counters that advance once an update with no clock at all: every second update while
//     an update is one tick;
//   - times authored in frames, stored as frames x step and then run down by the step: frames
//     x frame unit.
// With two or more ticks in an update (the shipped rate, or a slow update) every patched site
// computes what the shipped code does.
#include "valkyria_frame_rate.h"

#include <cstdio>

namespace ValkyriaFrameRate {
namespace {

using Bytes = std::vector<uint8_t>;

constexpr uint64_t Ticks = 0xa261e4; ///< ticks in this update, integer
constexpr uint64_t Unit = 0xa261e8;  ///< the same as a float: the frame unit
constexpr uint64_t Step = 0xa12fc0;  ///< ticks in this update x game speed
constexpr uint64_t One = 0x7c53cc;   ///< the game's read-only 1.0f
/// Nine of the thirteen alignment bytes between two functions, right before the event camera
/// update (FUN_003285a0): the helper its four patched sites call.
constexpr uint64_t CameraHelper = 0x328593;

void Put32(Bytes& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
Bytes Hex(const char* text) {
    Bytes out;
    for (; text[0] && text[1]; text += 2) {
        auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        out.push_back(static_cast<uint8_t>(nibble(text[0]) << 4 | nibble(text[1])));
    }
    return out;
}
void Append(Bytes& out, const Bytes& more) {
    out.insert(out.end(), more.begin(), more.end());
}
uint32_t Relative(uint64_t from_end, uint64_t to) {
    return static_cast<uint32_t>(static_cast<int64_t>(to) - static_cast<int64_t>(from_end));
}
/// A 4-byte opcode with a rip-relative operand (an 8-byte instruction at `at`) reading `from`.
Bytes Load(const char* opcode, uint64_t at, uint64_t from) {
    Bytes out = Hex(opcode);
    Put32(out, Relative(at + 8, from));
    return out;
}
/// call CameraHelper, the instruction at `at`.
Bytes CallHelper(uint64_t at) {
    Bytes out{0xe8};
    Put32(out, Relative(at + 5, CameraHelper));
    return out;
}

} // namespace

std::vector<Write> Writes() {
    std::vector<Write> writes;
    const auto add = [&](uint64_t at, Bytes before, Bytes after) {
        writes.push_back({at, std::move(before), std::move(after)});
    };
    const auto verify = [&](uint64_t at, const char* hex) { add(at, Hex(hex), Hex(hex)); };

    // The clock. The timing state starts with a minimum of one tick (shipped: the start-up
    // step truncated, 2) and keeps it: the scene object that puts back the tick count it was
    // created with (FUN_0044e110) no longer stores that count as the minimum, which after a
    // slow update would hold the game at 30.
    add(0x3d319f, Hex("c4e1fa2c1d18fe6300"), Hex("bb010000000f1f4000")); // mov ebx,1
    add(0x3d2fb9, Hex("c70002000000"), Hex("c70001000000"));             // mov dword [rax],1
    add(0x44e1b2, Hex("8901"), Hex("6690"));
    // The frame unit keeps the 2.0 the game starts with: the clock and that scene object no
    // longer rewrite it with the tick count.
    verify(0x3d2f9b, "c7054332650000000040"); // mov dword [0xa261e8],2.0f
    add(0x3d49c7, Hex("c5fa110519186500"), Hex("0f1f840000000000"));
    add(0x44e17a, Hex("c5fa110566805d00"), Hex("0f1f840000000000"));

    // "One authored frame is `ticks` ticks": timeline frame numbers to time (two loaders), the
    // timeline and trail sampler (FUN_001a3b00) and two per-frame speeds turned into rates.
    // mov eax/ecx,[ticks] becomes mov eax/ecx,2.
    for (const auto& [at, before] :
         {std::pair<uint64_t, const char*>{0x1a3b3d, "8b05a1268800"}, {0x1a3b7a, "8b0564268800"},
          {0x1a3bf5, "8b05e9258800"}, {0x1a3c4d, "8b0591258800"}, {0x1a45ad, "8b05311c8800"},
          {0x6e4aaf, "8b052f173400"}})
        add(at, Hex(before), Hex("b80200000090"));
    for (const auto& [at, before] :
         {std::pair<uint64_t, const char*>{0x1a2ae7, "8b0df7368800"}, {0x437e33, "8b0dabe35e00"},
          {0x1a3cdd, "8b0d01258800"}, {0x1a4641, "8b0d9d1b8800"}, {0x4c44c7, "8b0d171d5600"}})
        add(at, Hex(before), Hex("b90200000090"));

    // Animation controllers are created with a sampling interval of one shipped update
    // (1/29.97 s) and hold their pose between samples: one tick (1/59.94 s).
    add(0x16dc6e, Hex("86ab083d"), Hex("86ab883c"));

    // Head and eye turns take their time in authored frames from the step of the update that
    // starts them: the frame unit instead (vmovss xmm0,[step] -> [frame unit]).
    for (uint64_t at : {0x350d7c, 0x350e1a, 0x350ef8})
        add(at, Load("c5fa1005", at, Step), Load("c5fa1005", at, Unit));

    // Mouth movement: a voice line's loudness track (sound update, FUN_00417000) advances one
    // sample an update while the line plays. Rewritten in place: with one tick in the update,
    // every second update (the phase in the track entry's unused padding at +0xc).
    add(0x4191db,
        Hex("498b4638488b40104885c00f8474ffffff4a8d0ce84885c90f8467ffffff"
            "428b4ce818423b4ce81c0f8d57ffffff4a8d44e818ffc18908e949ffffff"),
        Hex("498b4638"         // mov rax,[r14+0x38]     the track table
            "488b4010"         // mov rax,[rax+0x10]     its entries
            "4885c0" "741f"    // test rax,rax ; jz done
            "4a8d04e8"         // lea rax,[rax+r13*8]    this line's entry
            "833df1cf600001"   // cmp dword [ticks],1
            "7707"             // ja advance             two or more ticks: as shipped
            "0fba780c00"       // btc dword [rax+0xc],0
            "720b"             // jc done
            "8b4818" "3b481c"  // advance: mov ecx,[rax+0x18] ; cmp ecx,[rax+0x1c]
            "7d03"             // jge done               at the end of the track
            "ff4018"           // inc dword [rax+0x18]
            "e954ffffff"       // done: jmp 0x419160
            "0f1f440000" "660f1f440000"));

    // Event camera (FUN_003285a0): four interpolators, each a length in authored frames and a
    // time, t = min(t + step / frame unit, length). A move one frame long is a cut: its only
    // shipped update lands on the new shot, but half a frame an update shows the midpoint
    // first. The first step of a move is one whole authored frame:
    //     t = min(max(t + step / frame unit, 1), length)
    // (the shipped values with two ticks). The sites are rewritten in place around a call to
    // vmaxss xmm1,xmm1,[1.0] ; ret.
    verify(One, "0000803f");
    {
        Bytes helper = Hex("c5f25f0d");
        Put32(helper, Relative(CameraHelper + 8, One));
        helper.push_back(0xc3);
        add(CameraHelper, Bytes(helper.size(), 0x90), helper);
        // r15 = r13 + 0x120 here; xmm2 is the length.
        Bytes first = Hex("c4c17a584f04"); // vaddss xmm1,xmm0,[r15+4]
        Append(first, CallHelper(0x32a5fd + 6));
        Append(first, Hex("c5f25dca"        // vminss xmm1,xmm1,xmm2
                          "c4c17a114f04"    // vmovss [r15+4],xmm1
                          "90"));
        add(0x32a5fd, Hex("c4c17a588d24010000" "c5f25dca" "c4c17a118d24010000"), first);
        const auto site = [&](uint64_t at, uint32_t offset, const char* before, const char* tail) {
            Bytes code = Hex("498d85"); // lea rax,[r13+offset]
            Put32(code, offset);
            Append(code, Hex("c5fa584804")); // vaddss xmm1,xmm0,[rax+4]
            Append(code, CallHelper(at + code.size()));
            Append(code, Hex(tail));
            code.push_back(0x90);
            add(at, Hex(before), code);
        };
        // vmovss xmm2,[rax] ; vminss xmm1,xmm1,xmm2 ; vmovss [rax+4],xmm1
        site(0x32a6b9, 0x180, "c4c17a588d84010000" "c4c17a109580010000" "c5f25dca" "c4c17a118d84010000",
             "c5fa1010" "c5f25dca" "c5fa114804");
        site(0x32a782, 0x200, "c4c17a588d04020000" "c4c17a109500020000" "c5f25dca" "c4c17a118d04020000",
             "c5fa1010" "c5f25dca" "c5fa114804");
        // vminss xmm0,xmm1,[rax] ; vmovss xmm1,[rax] ; vmovss [rax+4],xmm0
        site(0x32a84b, 0x248, "c4c17a58854c020000" "c4c17a108d48020000" "c5fa5dc1" "c4c17a11854c020000",
             "c5f25d00" "c5fa1008" "c5fa114004");
    }

    // Battle chat subtitles (the queued-message window, update FUN_0047cf10) fade by 32 an
    // update and, once the queue is empty, close again every ten updates until the window is
    // gone; each close restarts the backdrop's timed closing animation, which at ten 60 Hz
    // updates never finishes (the backdrop flickered until the window was destroyed). With
    // one tick in the update the fade and timer state machine runs every second update: the
    // phase is the top bit of the object's unused byte at +0x298, and the carry leaves through
    // the function's own return. The alpha byte comes from r12b (it is stored clamped), which
    // makes the room.
    {
        Bytes gate = Hex("833d"); // cmp dword [ticks],1 (the immediate follows the operand)
        Put32(gate, Relative(0x47cf83, Ticks));
        Append(gate, Hex("01"
                         "770a"             // ja run
                         "4180869802000080" // add byte [r14+0x298],0x80
                         "7217"             // jc 0x47cfa6 (jb 0x47d163: return)
                         "458bbe90020000"   // run: mov r15d,[r14+0x290]    the fade step
                         "eb54" "90"));     // jmp 0x47cfec
        add(0x47cf7c,
            Hex("458bbe90020000" "4585ff" "7411" "458ba68c020000" "4585e4" "7e58" "4588e5" "eb56"), gate);
        add(0x47cfec, Hex("4531ed" "4181fc00010000" "7c03" "41b5ff"),
            Hex("458ba68c020000" // mov r12d,[r14+0x28c]    the alpha
                "4585ff" "74a1"  // test r15d,r15d ; je 0x47cf99    not fading: the timer
                "0f1f00"));
        for (uint64_t at : {0x47d01d, 0x47d038}) // mov [rax+0x15b],r13b -> r12b
            add(at, Hex("4488a85b010000"), Hex("4488a05b010000"));
    }

    // Display times authored in frames, stored as frames x step and run down by the step
    // (text stayed half as long): talk, txt_enter_instruction, txt_enter_message and
    // if_enter_advice_window of the mission scripts, and the timed window of FUN_006865c0.
    // vmulss xmm0,xmm0,[step] -> [frame unit].
    for (uint64_t at : {0x36c87d, 0x36d19c, 0x36d56e, 0x3a97f2, 0x686625})
        add(at, Load("c5fa5905", at, Step), Load("c5fa5905", at, Unit));
    return writes;
}

bool Start(uintptr_t base, std::string* problem) {
    const std::vector<Write> writes = Writes();
    if (!ValkyriaWidePatches::Matches(base, writes, problem)) return false;
    ValkyriaWidePatches::Apply(base, writes);
    std::printf("Valkyria 60 FPS (experimental): on, %zu startup patches\n", writes.size());
    return true;
}

} // namespace ValkyriaFrameRate
