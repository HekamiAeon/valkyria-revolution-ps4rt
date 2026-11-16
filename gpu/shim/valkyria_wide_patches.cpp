// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) wide canvas startup patches (valkyria_wide_patches.h). Every
// site was verified in the v1.00 eboot; each write carries the original bytes it replaces and
// nothing is written unless all of them match.
#include "valkyria_wide_patches.h"

#include <sys/uio.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>

#include "valkyria_wide_ui.h"

namespace ValkyriaWidePatches {
namespace {

using Bytes = std::vector<uint8_t>;

void Put32(Bytes& out, uint32_t value) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(value >> (8 * i)));
}
uint32_t Float(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}
Bytes Hex(const char* text) {
    Bytes out;
    for (; text[0] && text[1]; text += 2) {
        auto nibble = [](char c) { return c <= '9' ? c - '0' : c - 'a' + 10; };
        out.push_back(static_cast<uint8_t>(nibble(text[0]) << 4 | nibble(text[1])));
    }
    return out;
}
/// mov esi,width; mov edx,height (be imm32 ba imm32).
Bytes MoveSize(uint32_t width, uint32_t height) {
    Bytes out{0xbe};
    Put32(out, width);
    out.push_back(0xba);
    Put32(out, height);
    return out;
}
int32_t Relative(uint64_t from_end, uint64_t to) {
    return static_cast<int32_t>(static_cast<int64_t>(to) - static_cast<int64_t>(from_end));
}

// Unused executable padding after the code segment (it ends at 0x8b3f48; the page ends at
// 0x8b4000): the two stubs below live there, inside the eboot's own code page.
constexpr uint64_t SceneStub = 0x8b3f50;
constexpr uint64_t ColourStub = 0x8b3fd0;
/// Read-only data in the same padding: the screen size in GUI units for the screen fade.
constexpr uint64_t FadeExtent = 0x8b3ff8;
constexpr uint64_t PaddingEnd = 0x8b4000;

/// Called instead of FUN_003d5d60 by its only caller (0x3d6699), right after the game copied
/// its 1920x1080 scene template (0x7f0fc0 -> 0xa12f20), chose the GUI scale and sized the
/// graphics arena at 1920x1080: when the extent is still stock it sets the shared scene
/// extent, its half size and the GUI root scale (height / 1080) and clears the PS4 Pro UI
/// flag, then continues to FUN_003d5d60. Exactly what the verified debugger patch did there.
Bytes SceneCode(int width, int height) {
    Bytes code;
    const auto here = [&] { return SceneStub + code.size(); };
    const auto compare = [&](uint64_t at, uint32_t value) { // cmp dword [rip+at],value
        code.insert(code.end(), {0x81, 0x3d});
        Put32(code, Relative(here() + 8, at));
        Put32(code, value);
    };
    const auto store = [&](uint64_t at, uint32_t value) { // mov dword [rip+at],value
        code.insert(code.end(), {0xc7, 0x05});
        Put32(code, Relative(here() + 8, at));
        Put32(code, value);
    };
    const float fw = static_cast<float>(width), fh = static_cast<float>(height), gui = fh / 1080.f;
    compare(0xa12f54, Float(1920));
    code.insert(code.end(), {0x75, 0}); // jne done
    const size_t first_skip = code.size() - 1;
    compare(0xa12f58, Float(1080));
    code.insert(code.end(), {0x75, 0});
    const size_t second_skip = code.size() - 1;
    for (const auto& [at, value] :
         {std::pair<uint64_t, float>{0xa12f54, fw}, {0xa12f58, fh}, {0xa12f64, fw},
          {0xa12f68, fh}, {0xa12f70, fw / 2}, {0xa12f74, fh / 2}, {0x968c80, gui},
          {0x968c84, gui}})
        store(at, Float(value));
    code.insert(code.end(), {0x80, 0x25}); // and byte [rip+0xa1279c],0xfe
    Put32(code, Relative(here() + 5, 0xa1279c));
    code.push_back(0xfe);
    code[first_skip] = static_cast<uint8_t>(code.size() - first_skip - 1);
    code[second_skip] = static_cast<uint8_t>(code.size() - second_skip - 1);
    code.push_back(0xe9); // done: jmp FUN_003d5d60
    Put32(code, Relative(here() + 4, 0x3d5d60));
    return code;
}

/// Called instead of FUN_001ce860 (the scene colour object constructor) by its only caller,
/// the factory wrapper at 0x1ce820: a stock 1920x1080 extent becomes the canvas.
Bytes ColourCode(int width, int height) {
    Bytes code = Hex("81fa80070000" "7512" "81f938040000" "750a"); // cmp edx,1920 / cmp ecx,1080
    code.push_back(0xba);
    Put32(code, width); // mov edx,width
    code.push_back(0xb9);
    Put32(code, height); // mov ecx,height
    code.push_back(0xe9);
    Put32(code, Relative(ColourStub + code.size() + 4, 0x1ce860));
    return code;
}

Bytes WithTail(Bytes head, const Bytes& tail) {
    head.insert(head.end(), tail.begin(), tail.end());
    return head;
}

} // namespace

bool Wide(int width, int height) {
    return (height == 1080 || height == 1440) && width > 0 && width % 8 == 0 &&
           width * 9 > height * 16 && width * 9 <= height * 32;
}

bool Supported(int width, int height) {
    const bool standard = (height == 576 || height == 720 || height == 1440 || height == 2160) &&
                          width * 9 == height * 16;
    return standard || Wide(width, height);
}

std::vector<Write> Writes(int width, int height) {
    if (!Supported(width, height)) return {};
    const uint32_t w = static_cast<uint32_t>(width), h = static_cast<uint32_t>(height);
    std::vector<Write> writes;
    const auto add = [&](uint64_t at, Bytes before, Bytes after) {
        writes.push_back({at, std::move(before), std::move(after)});
    };
    const auto verify = [&](uint64_t at, const char* hex) { add(at, Hex(hex), Hex(hex)); };
    const Bytes stock_size = MoveSize(1920, 1080);
    // Both menu snapshots (FUN_0020a5xx), allocated before the menu captures the scene.
    add(0x20a5ee, stock_size, MoveSize(w, h));
    add(0x20a65b, stock_size, MoveSize(w, h));
    // The queued blur viewport: movabs rax,(1920.f,1080.f).
    {
        Bytes after = Hex("48b8");
        Put32(after, Float(width));
        Put32(after, Float(height));
        add(0x20986b, Hex("48b80000f04400008744"), after);
    }
    // Both transition snapshots and their private copy rectangles (each rectangle's only
    // reader is its snapshot callback): allocation and copy stay in sync.
    add(0x20c390, stock_size, MoveSize(w, h));
    add(0x20c79b, stock_size, MoveSize(w, h));
    for (uint64_t at : {0x7b3a30, 0x7b3a40}) {
        Bytes before(8, 0), after(8, 0);
        Put32(before, Float(1920));
        Put32(before, Float(1080));
        Put32(after, Float(width));
        Put32(after, Float(height));
        add(at, before, after);
    }
    // Mission-table map layer (FUN_006b1920): when the viewport is at least as tall as the
    // authored 2048x1434 layer, take the game's own 1440 UI branch (2048x1440, scale 1); Gnm
    // render targets reject the stock branch's non-multiple-of-8 height. At 1080 high the
    // stock 1542x1080 branch still runs.
    verify(0x6b1994, "c5fa100d");
    add(0x6b19f8, Hex("7304"), Hex("739a"));
    // Radial blur (event FUN_001a1400, script setter 0x1db3b0, tween 0x1ccde4) keeps its
    // centre and radii in 1920x1080 virtual pixels, but the constant setup divides them by the
    // scene size [r8+0xd0]. Load the read-only (1920,1080) pair at 0x7fb950 instead
    // (mov rcx,[rip+0x63db8d]): the blur shader gets exactly its 1080p constants.
    verify(0x7fb950, "8007000038040000");
    add(0x1bddbc, Hex("498b88d0000000c5"), Hex("488b0d8ddb6300c5"));
    // Its event setter (FUN_001a1400) turns normalized centre keys into those pixels with the
    // live scene size; the default 960/W, 540/H only round-trips to 960,540. Scale by the
    // read-only 1920x1080 scene template floats (0x7f0ff4) instead, so animated centres stay
    // in the same virtual pixels.
    add(0x1a14ea, Hex("c5fa100d621a8700"), Hex("c5fa100d02fb6400"));
    add(0x1a14f9, Hex("c5fa1005571a8700"), Hex("c5fa1005f7fa6400"));
    // Screen fades (VlIfLayerFilter draw FUN_0020b8d0, solid submit at 0x20bb1b) size their
    // full-screen quad from the live screen struct [0x9cda98] (pixels, filled from the scene
    // extent at 0x22fb7a) but draw it under the GUI root, which scales by height / 1080. The
    // game's other readers of that struct divide it by the GUI scale first (0x5642fb,
    // 0x68526d); the fade does not, so below 1080 it covered only the top-left (H/1080)^2 of
    // the screen and the scene behind it showed (observed at 1024x576 from startup). Point its
    // struct load at the screen size in GUI units instead, W * 1080 / H by 1080: the screen
    // exactly, at stock 1920x1080 the same values. lea rcx,[rip+FadeExtent] replaces
    // mov rcx,[rip+0x9cda98]; the following loads read width and height from rcx as before.
    {
        Bytes lea = Hex("488d0d");
        Put32(lea, Relative(0x20b9d0, FadeExtent));
        add(0x20b9c9, Hex("488b0dc8207c00"), lea);
        verify(0x20b9d8, "448b018b4904"); // mov r8d,[rcx]; mov ecx,[rcx+4]
        Bytes extent;
        Put32(extent, Float(static_cast<float>(1080.0 * width / height)));
        Put32(extent, Float(1080));
        add(FadeExtent, Bytes(8, 0), extent);
    }
    // The game's display buffer defaults (1920x1080, pitch 1920), set before its allocation;
    // the PS4 Pro table path after them does not run here.
    {
        Bytes after = Hex("48b8");
        Put32(after, w);
        Put32(after, h);
        add(0x74fdf5, Hex("48b8800700003804000049894630"), WithTail(after, Hex("49894630")));
        Bytes pitch = Hex("41c74638");
        Put32(pitch, w);
        add(0x74fe03, Hex("41c7463880070000"), pitch);
    }
    // Shared scene extent and GUI scale, then the scene colour object: stubs in the code
    // page padding, entered through their callers' rel32 calls.
    const Bytes scene = SceneCode(width, height), colour = ColourCode(width, height);
    if (SceneStub + scene.size() > ColourStub || ColourStub + colour.size() > FadeExtent) return {};
    add(SceneStub, Bytes(scene.size(), 0), scene);
    add(ColourStub, Bytes(colour.size(), 0), colour);
    {
        Bytes call{0xe8};
        Put32(call, Relative(0x3d669e, SceneStub));
        add(0x3d6699, Hex("e8c2f6ffff"), call);
        Bytes colour_call{0xe8};
        Put32(colour_call, Relative(0x1ce852, ColourStub));
        add(0x1ce84d, Hex("e80e000000"), colour_call);
    }
    // The scene template the stub expects to find copied (0x7f0fc0 -> 0xa12f20): full and half
    // extents.
    verify(0x7f0ff4, "0000f04400008744");
    verify(0x7f1004, "0000f04400008744");
    verify(0x7f1010, "0000704400000744");
    return writes;
}

bool Matches(uintptr_t base, const std::vector<Write>& writes, std::string* problem) {
    for (const Write& write : writes) {
        if (write.before.size() != write.after.size() || write.before.empty()) {
            if (problem) *problem = "malformed patch";
            return false;
        }
        Bytes observed(write.before.size());
        iovec local{observed.data(), observed.size()};
        iovec remote{reinterpret_cast<void*>(base + write.at), observed.size()};
        if (process_vm_readv(getpid(), &local, 1, &remote, 1, 0) !=
                static_cast<ssize_t>(observed.size()) ||
            observed != write.before) {
            if (problem) {
                char text[96];
                std::snprintf(text, sizeof(text), "game code differs at 0x%llx",
                              static_cast<unsigned long long>(write.at));
                *problem = text;
            }
            return false;
        }
    }
    return true;
}

void Apply(uintptr_t base, const std::vector<Write>& writes) {
    for (const Write& write : writes)
        std::memcpy(reinterpret_cast<void*>(base + write.at), write.after.data(),
                    write.after.size());
}

bool Start(uintptr_t base, int width, int height, std::string* problem) {
    std::vector<Write> writes = Writes(width, height);
    if (writes.empty()) {
        if (problem) *problem = "unsupported size";
        return false;
    }
    for (const Write& write : writes)
        if ((write.at >= SceneStub && write.at < PaddingEnd) &&
            write.at + write.after.size() > PaddingEnd) {
            if (problem) *problem = "stub exceeds the code page";
            return false;
        }
    if (!Matches(base, writes, problem)) return false;
    if (valkyria_wide_ui_check(base)) {
        if (problem) *problem = "UI hook signatures differ";
        return false;
    }
    // The UI hooks first: they change nothing unless all are installed. The verified patches
    // cannot fail after Matches. At 16:9 the hooks have no margin to apply: only full-screen
    // copies in stock pixels and the narration paper change.
    if (valkyria_wide_ui_install(base, static_cast<unsigned>(width), static_cast<unsigned>(height))) {
        if (problem) *problem = "UI hooks could not be installed";
        return false;
    }
    Apply(base, writes);
    std::printf("Valkyria internal resolution: %dx%d scene, display buffer and snapshots (GUI "
                "scale %.4g, side margin %g)\n",
                width, height, height / 1080.0, (width - 1920.0 * height / 1080.0) / 2);
    return true;
}

} // namespace ValkyriaWidePatches
