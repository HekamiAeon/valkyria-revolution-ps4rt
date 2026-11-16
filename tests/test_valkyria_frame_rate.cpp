// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria 60 FPS startup patches on a synthetic image (no game data): well-formed writes that
// stay clear of the canvas patches, all-or-nothing verification, the rewritten fragments
// (event camera, mouth track, subtitle window) executed at one and two ticks an update, and
// the camera clip hooks on synthetic clips: a cut keeps its earlier frame.
// Optional argument: a decrypted eboot.elf, checked read-only to match every patch's original
// bytes.
#include <sys/mman.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "gpu/shim/valkyria_camera_cuts.h"
#include "gpu/shim/valkyria_frame_rate.h"

using ValkyriaFrameRate::Start;
using ValkyriaFrameRate::Write;
using ValkyriaFrameRate::Writes;

// Enters guest code at `entry` with r12-r15, xmm0-xmm2 and rax from `regs`, and stores them
// back when it returns.
struct Regs {
    uint64_t r12, r13, r14, r15;
    float xmm0, xmm1, xmm2, unused;
    uint64_t rax;
};
extern "C" void valkyria_enter(const void* entry, Regs* regs);
asm(R"(
    .text
    .globl valkyria_enter
    .type valkyria_enter,@function
valkyria_enter:
    push %rbx
    push %r12
    push %r13
    push %r14
    push %r15
    push %rbp
    sub $8,%rsp
    mov %rsi,%rbx
    mov 0(%rbx),%r12
    mov 8(%rbx),%r13
    mov 16(%rbx),%r14
    mov 24(%rbx),%r15
    movss 32(%rbx),%xmm0
    movss 36(%rbx),%xmm1
    movss 40(%rbx),%xmm2
    mov 48(%rbx),%rax
    call *%rdi
    mov %r12,0(%rbx)
    mov %r13,8(%rbx)
    mov %r14,16(%rbx)
    mov %r15,24(%rbx)
    movss %xmm0,32(%rbx)
    movss %xmm1,36(%rbx)
    movss %xmm2,40(%rbx)
    mov %rax,48(%rbx)
    add $8,%rsp
    pop %rbp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbx
    ret
    .size valkyria_enter,.-valkyria_enter
)");

namespace {

constexpr size_t ImageSize = 0xa30000;
constexpr uint64_t Ticks = 0xa261e4, Unit = 0xa261e8, One = 0x7c53cc, CodeEnd = 0x8b3f48;

struct Image {
    uint8_t* data = static_cast<uint8_t*>(mmap(nullptr, ImageSize, PROT_READ | PROT_WRITE | PROT_EXEC,
                                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Image() { assert(data != MAP_FAILED); }
    ~Image() { munmap(data, ImageSize); }
    uintptr_t base() const { return reinterpret_cast<uintptr_t>(data); }
    uint32_t& u(uint64_t at) { return *reinterpret_cast<uint32_t*>(data + at); }
    void put(uint64_t at, std::initializer_list<uint8_t> bytes) {
        std::copy(bytes.begin(), bytes.end(), data + at);
    }
    /// mov eax,value ; ret: where a fragment leaves.
    void leave(uint64_t at, uint8_t value) { put(at, {0xb8, value, 0, 0, 0, 0xc3}); }
    bool has(const Write& write, bool after) const {
        const auto& bytes = after ? write.after : write.before;
        return std::equal(bytes.begin(), bytes.end(), data + write.at);
    }
};

/// An image with every patch's original bytes (as the game build has them).
void Stock(Image& image) {
    for (const Write& write : Writes())
        std::copy(write.before.begin(), write.before.end(), image.data + write.at);
}

/// The patched image, the game's read-only 1.0 included (it is one of the verified bytes).
void Patched(Image& image) {
    Stock(image);
    std::string problem;
    assert(Start(image.base(), &problem));
}

bool Hooked(const Image& image, uint64_t at) {
    return image.data[at] == 0xff && image.data[at + 1] == 0x25 && image.data[at + 14] == 0x90;
}
bool HookSite(const Write& write) {
    return write.at == ValkyriaCameraCuts::MotionSampler || write.at == ValkyriaCameraCuts::ViewSampler;
}

int32_t Disp(const Image& image, uint64_t at) {
    int32_t value;
    std::memcpy(&value, image.data + at, 4);
    return value;
}

void TestWrites() {
    std::vector<Write> writes = Writes();
    assert(!writes.empty());
    std::sort(writes.begin(), writes.end(), [](const Write& a, const Write& b) { return a.at < b.at; });
    for (size_t i = 0; i < writes.size(); ++i) {
        const Write& write = writes[i];
        assert(!write.before.empty() && write.before.size() == write.after.size());
        // Game code and its read-only data only: nothing in the code page's tail, where the
        // canvas patches keep their stubs.
        assert(write.at >= 0x1000 && write.at + write.after.size() <= CodeEnd);
        assert(i == 0 || writes[i - 1].at + writes[i - 1].after.size() <= write.at);
    }
    // Clear of every canvas patch and of the UI hooks' prologues (valkyria_wide_ui.c).
    std::vector<std::pair<uint64_t, uint64_t>> taken = {
        {0x764860, 0x764860 + 32}, {0x765750, 0x765750 + 32}, {0x235ce0, 0x235ce0 + 32}};
    for (auto [w, h] : {std::pair{2560, 1080}, {3440, 1440}, {3840, 1440}, {3840, 1080}, {5120, 1440},
                        {1024, 576}, {1280, 720}, {2560, 1440}, {3840, 2160}})
        for (const Write& write : ValkyriaWidePatches::Writes(w, h))
            taken.emplace_back(write.at, write.at + write.after.size());
    for (const Write& write : writes)
        for (const auto& [from, to] : taken) assert(write.at + write.after.size() <= from || write.at >= to);
    std::puts("PASS: writes are well formed, apart, inside the game's code and clear of the canvas patches");
}

void TestStart() {
    Image image;
    Stock(image);
    std::string problem;
    assert(Start(image.base(), &problem) && problem.empty());
    for (const Write& write : Writes()) assert(HookSite(write) ? Hooked(image, write.at) : image.has(write, true));
    // The clock: a minimum of one tick, the frame unit left at the game's 2.0.
    assert(image.data[0x3d319f] == 0xbb && image.u(0x3d31a0) == 1 && image.u(0x3d2fbb) == 1);
    // Operands: the pinned times read the frame unit, the gates the tick count, the camera
    // sites call the helper and the helper reads 1.0.
    for (uint64_t at : {0x350d7c, 0x350e1a, 0x350ef8, 0x36c87d, 0x36d19c, 0x36d56e, 0x3a97f2, 0x686625})
        assert(at + 8 + Disp(image, at + 4) == Unit);
    assert(0x4191db + 0x18 + Disp(image, 0x4191db + 0x13) == Ticks);
    assert(0x4191db + 0x31 + Disp(image, 0x4191db + 0x2d) == 0x419160);
    assert(0x47cf83 + Disp(image, 0x47cf7e) == Ticks);
    const uint64_t helper = 0x328593;
    assert(helper + 8 + Disp(image, helper + 4) == One && image.data[helper + 8] == 0xc3);
    for (uint64_t call : {0x32a5fd + 6, 0x32a6b9 + 12, 0x32a782 + 12, 0x32a84b + 12})
        assert(image.data[call] == 0xe8 && call + 5 + Disp(image, call + 1) == helper);
    // A second start finds its own bytes and changes nothing.
    assert(!Start(image.base(), &problem) && problem.rfind("game code differs at 0x", 0) == 0);
    for (const Write& write : Writes()) assert(HookSite(write) ? Hooked(image, write.at) : image.has(write, true));
    std::puts("PASS: start writes every patch and both camera hooks; operands and calls reach their targets");
}

void TestFailClosed() {
    const std::vector<Write> writes = Writes();
    for (const Write& odd : writes) {
        Image image;
        Stock(image);
        image.data[odd.at + odd.before.size() - 1] ^= 0x40;
        std::string problem;
        assert(!Start(image.base(), &problem));
        char expected[64];
        std::snprintf(expected, sizeof(expected), "game code differs at 0x%llx",
                      static_cast<unsigned long long>(odd.at));
        assert(problem == expected);
        for (const Write& write : writes)
            assert(&write == &odd || write.before == write.after || image.has(write, true) == false);
        for (const Write& write : writes) assert(&write == &odd || image.has(write, false));
        assert(odd.at == ValkyriaCameraCuts::MotionSampler || !Hooked(image, ValkyriaCameraCuts::MotionSampler));
    }
    void* hole = mmap(nullptr, ImageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(hole != MAP_FAILED);
    std::string problem;
    assert(!Start(reinterpret_cast<uintptr_t>(hole), &problem) && !problem.empty());
    munmap(hole, ImageSize);
    std::puts("PASS: any mismatched byte or an unreadable image leaves the game unchanged");
}

/// One camera interpolator update: delta = step / frame unit; returns the new time.
float Camera(Image& image, uint64_t site, uint32_t offset, float time, float delta, float length) {
    float object[0x300 / 4] = {};
    object[offset / 4] = length;
    object[offset / 4 + 1] = time;
    Regs regs{};
    regs.r13 = reinterpret_cast<uint64_t>(object);
    regs.r15 = regs.r13 + 0x120;
    regs.xmm0 = delta;
    regs.xmm2 = site == 0x32a5fd ? length : -1; // loaded before the first site only
    valkyria_enter(image.data + site, &regs);
    const float result = object[offset / 4 + 1];
    // What the code after each site compares: the new time and the length.
    if (site == 0x32a84b) assert(regs.xmm0 == result && regs.xmm1 == length);
    else assert(regs.xmm1 == result && regs.xmm2 == length && regs.xmm0 == delta);
    for (size_t i = 0; i < std::size(object); ++i)
        assert(i == offset / 4 || i == offset / 4 + 1 || object[i] == 0);
    assert(object[offset / 4] == length);
    return result;
}

void TestCamera() {
    Image image;
    Patched(image);
    const std::pair<uint64_t, uint32_t> sites[] = {
        {0x32a5fd, 0x120}, {0x32a6b9, 0x180}, {0x32a782, 0x200}, {0x32a84b, 0x248}};
    const uint64_t ends[] = {0x32a5fd + 22, 0x32a6b9 + 31, 0x32a782 + 31, 0x32a84b + 31};
    for (uint64_t end : ends) image.put(end, {0xc3});
    for (const auto& [site, offset] : sites) {
        // One tick an update (half an authored frame): a one-frame move is a cut at once and
        // a longer move starts with one whole frame, then runs at its authored speed.
        assert(Camera(image, site, offset, 0, 0.5f, 1) == 1);
        assert(Camera(image, site, offset, 0, 0.5f, 10) == 1);
        assert(Camera(image, site, offset, 1, 0.5f, 10) == 1.5f);
        assert(Camera(image, site, offset, 9.75f, 0.5f, 10) == 10);
        assert(Camera(image, site, offset, 10, 0.5f, 10) == 10);
        // Two ticks or more: min(t + delta, length), the shipped result.
        for (float length : {0.f, 0.5f, 1.f, 10.f})
            for (float time : {0.f, 0.5f, 1.f, 3.f, 9.5f, 10.f})
                for (float delta : {1.f, 1.5f, 2.f})
                    assert(time > length ||
                           Camera(image, site, offset, time, delta, length) == std::min(time + delta, length));
    }
    std::puts("PASS: camera moves start with a whole authored frame; unchanged at two ticks");
}

void TestMouthTrack() {
    Image image;
    Patched(image);
    image.put(0x419160, {0xc3}); // the loop the fragment returns to
    struct Entry { uint32_t a, b, c, phase, d, e, position, length; }; // +0xc, +0x18, +0x1c
    Entry entries[3] = {};
    struct { uint64_t count, unused, entries; } table = {3, 0, reinterpret_cast<uint64_t>(entries)};
    uint64_t owner[8] = {};
    owner[7] = reinterpret_cast<uint64_t>(&table); // +0x38
    const auto update = [&](unsigned entry) {
        Regs regs{};
        regs.r14 = reinterpret_cast<uint64_t>(owner);
        regs.r13 = entry * sizeof(Entry) / 8;
        valkyria_enter(image.data + 0x4191db, &regs);
    };
    entries[1].length = 4;
    image.u(Ticks) = 2; // the shipped rate: one sample an update, up to the end
    for (uint32_t expected : {1u, 2u, 3u, 4u, 4u, 4u}) {
        update(1);
        assert(entries[1].position == expected && entries[1].phase == 0);
    }
    entries[1].position = 0;
    image.u(Ticks) = 1; // every second update
    for (uint32_t expected : {1u, 1u, 2u, 2u, 3u, 3u, 4u, 4u, 4u, 4u}) {
        update(1);
        assert(entries[1].position == expected);
    }
    image.u(Ticks) = 3; // a slow update between them: as shipped
    entries[1].position = 1;
    update(1);
    assert(entries[1].position == 2);
    assert(entries[0].position == 0 && entries[2].position == 0 && entries[0].phase == 0 && entries[2].phase == 0);
    table.entries = 0; // no tracks: nothing read or written
    update(1);
    assert(entries[1].position == 2);
    std::puts("PASS: the mouth track advances every update at two ticks, every second at one");
}

void TestSubtitleWindow() {
    Image image;
    Patched(image);
    // Where the gate leaves: 1 the timer (not fading), 2 the function's return, 3 the fade.
    image.leave(0x47cf99, 1);
    image.leave(0x47cfa6, 2);
    image.leave(0x47cffb, 3);
    uint8_t object[0x2b0] = {};
    const auto word = [&](size_t at) -> int32_t& { return *reinterpret_cast<int32_t*>(object + at); };
    const auto update = [&](Regs& regs) {
        regs = {};
        regs.r12 = regs.r15 = 0x1111111111111111;
        regs.r14 = reinterpret_cast<uint64_t>(object);
        valkyria_enter(image.data + 0x47cf7c, &regs);
        return static_cast<int>(regs.rax);
    };
    Regs regs;
    word(0x28c) = 96, word(0x290) = -32;
    image.u(Ticks) = 2; // the shipped rate: every update, the phase byte untouched
    for (int i = 0; i < 3; ++i) {
        assert(update(regs) == 3 && uint32_t(regs.r12) == 96 && int32_t(regs.r15) == -32 && object[0x298] == 0);
    }
    word(0x290) = 0;
    assert(update(regs) == 1 && object[0x298] == 0);
    image.u(Ticks) = 1; // every second update; the others return at once
    word(0x290) = 32;
    for (int i = 0; i < 6; ++i) {
        const int left = update(regs);
        if (i % 2 == 0) assert(left == 3 && uint32_t(regs.r12) == 96 && int32_t(regs.r15) == 32 && object[0x298] == 0x80);
        else assert(left == 2 && regs.r12 == 0x1111111111111111 && object[0x298] == 0);
    }
    word(0x290) = 0;
    assert(update(regs) == 1 && update(regs) == 2 && update(regs) == 1);
    for (size_t i = 0; i < sizeof(object); ++i)
        assert((i >= 0x28c && i < 0x294) || i == 0x298 || object[i] == 0);
    // The alpha byte the two widgets get is r12b: mov [rax+0x15b],r12b.
    for (uint64_t at : {0x47d01d, 0x47d038}) {
        image.put(at + 7, {0xc3});
        uint8_t widget[0x160] = {};
        Regs store{};
        store.r12 = 0x1c8;
        store.rax = reinterpret_cast<uint64_t>(widget);
        valkyria_enter(image.data + at, &store);
        assert(widget[0x15b] == 0xc8 && widget[0x15a] == 0 && widget[0x15c] == 0);
    }
    std::puts("PASS: the subtitle window's fade runs every update at two ticks, every second at one");
}

using Pose = std::array<float, ValkyriaCameraCuts::PoseSize>;

Pose Shot(float px, float py, float pz, float tx, float ty, float tz, float roll = 0) {
    Pose pose{};
    pose[0] = px, pose[1] = py, pose[2] = pz, pose[4] = tx, pose[5] = ty, pose[6] = tz, pose[14] = roll;
    return pose;
}

void TestCutRules() {
    using ValkyriaCameraCuts::MotionCut;
    using ValkyriaCameraCuts::ViewCut;
    const auto motion = [](const Pose& p, const Pose& a, const Pose& b, const Pose& q) {
        return MotionCut(p.data(), a.data(), b.data(), q.data());
    };
    // Looking 10 units ahead from x: a dolly along x.
    const auto at = [](float x) { return Shot(x, 0, 0, x, 0, 10); };
    const Pose other = Shot(40, 5, -20, 30, 0, 0);
    assert(motion(at(0), at(0), other, other));                    // between two still shots
    assert(motion(at(0), at(0.1f), other, Shot(40.1f, 5, -20, 30, 0, 0))); // between two moving shots
    assert(motion(at(0), at(0), Shot(0, 0, 0, 8, 0, 6), Shot(0, 0, 0, 8, 0, 6))); // turned on the spot
    assert(motion(at(0), at(0), Shot(0, 0, 0, 0, 0, 10, 1), Shot(0, 0, 0, 0, 0, 10, 1))); // rolled over
    assert(!motion(at(0), at(0), at(0), at(0)));                   // still
    assert(!motion(at(0), at(0.1f), at(0.2f), at(0.3f)));          // steady move
    assert(!motion(at(0), at(0), at(0.01f), at(0.04f)));           // easing in from rest
    assert(!motion(at(0.04f), at(0.01f), at(0), at(0)));           // easing out to rest
    assert(!motion(at(0), at(0), at(0.015625f), at(0.015625f)));   // one quantised step of a slow drift
    assert(!motion(at(0), at(0), at(0.3f), at(0.3f)));             // a nudge too small to be a new shot
    assert(!motion(at(0), at(3), at(6), at(9)));                   // a fast, even pan
    assert(!motion(at(0), at(0.5f), at(-0.3f), at(0.4f)));         // a shaking camera
    assert(!motion(Shot(0, 0, 0, 0, 0, 10, 0.f), Shot(0, 0, 0, 0, 0, 10, 0.02f),
                   Shot(0, 0, 0, 0, 0, 10, 0.04f), Shot(0, 0, 0, 0, 0, 10, 0.06f))); // a slow roll
    const auto view = [](float p, float a, float b, float q) {
        Pose poses[4] = {};
        const float values[4] = {p, a, b, q};
        for (int i = 0; i < 4; ++i) poses[i][4] = values[i], poses[i][7] = 1, poses[i][9] = 0.5f;
        return ViewCut(poses[0].data(), poses[1].data(), poses[2].data(), poses[3].data());
    };
    assert(view(30, 30, 55, 55) && view(30, 30.5f, 55, 54.5f));
    assert(!view(30, 30, 30, 30) && !view(30, 32, 34, 36) && !view(30, 30, 30.5f, 30.5f));
    assert(!view(0, 0, 0.004f, 0.004f) && !view(30, 30, 31, 33));
    std::puts("PASS: a cut is a one-frame step out of line with the motion around it");
}

/// A synthetic clip sampler behind the shipped entry bytes: calls back with (clip, time).
struct Track {
    std::vector<Pose> frames;
    size_t offset = 0; ///< where the pose goes in the clip
    int calls = 0;
    void sample(uint64_t* clip, float time) {
        ++calls;
        uint8_t* const motion = reinterpret_cast<uint8_t*>(clip[1]);
        if (!motion) return; // not loaded yet
        time = std::clamp(time, 0.f, float(frames.size() - 1));
        std::memcpy(motion + 0x70, &time, 4);
        const size_t frame = size_t(time);
        const Pose& a = frames[frame];
        const Pose& b = frames[std::min(frame + 1, frames.size() - 1)];
        Pose pose;
        for (size_t i = 0; i < pose.size(); ++i) pose[i] = a[i] + (b[i] - a[i]) * (time - float(frame));
        std::memcpy(reinterpret_cast<uint8_t*>(clip) + offset, pose.data(), sizeof(pose));
    }
};
Track motion_track, view_track;
void MotionCallback(uint64_t* clip, float time) { motion_track.sample(clip, time); }
void ViewCallback(uint64_t* clip, float time) { view_track.sample(clip, time); }

void PlantSampler(Image& image, uint64_t at, void (*callback)(uint64_t*, float)) {
    uint8_t* code = image.data + at + sizeof(ValkyriaCameraCuts::SamplerEntry);
    const uint8_t head[] = {0x48, 0x89, 0xdf,        // mov rdi,rbx
                            0xc5, 0xf8, 0x28, 0xc2,  // vmovaps xmm0,xmm2
                            0x48, 0xb8};             // movabs rax,callback
    const uint8_t tail[] = {0xff, 0xd0,              // call rax
                            0x48, 0x83, 0xc4, 0x08,  // add rsp,8
                            0x5b, 0x5d, 0xc3};       // pop rbx ; pop rbp ; ret
    std::memcpy(code, head, sizeof(head));
    std::memcpy(code + sizeof(head), &callback, 8);
    std::memcpy(code + sizeof(head) + 8, tail, sizeof(tail));
}

void TestCameraHooks() {
    Image image;
    Patched(image);
    PlantSampler(image, ValkyriaCameraCuts::MotionSampler, MotionCallback);
    PlantSampler(image, ValkyriaCameraCuts::ViewSampler, ViewCallback);
    const auto sample_motion = reinterpret_cast<void (*)(uint64_t*, float)>(image.data + ValkyriaCameraCuts::MotionSampler);
    const auto sample_view = reinterpret_cast<void (*)(uint64_t*, float)>(image.data + ValkyriaCameraCuts::ViewSampler);
    // Frames 0-3: a slow dolly. Frames 4-7: another shot, also moving. The cut is 3 -> 4.
    motion_track.offset = 0x10;
    for (int i = 0; i < 4; ++i) motion_track.frames.push_back(Shot(0.1f * i, 0, 0, 0.1f * i, 0, 10));
    for (int i = 4; i < 8; ++i) motion_track.frames.push_back(Shot(40 + 0.1f * i, 5, -20, 30, 0, 0));
    uint8_t motion[0x80] = {};
    uint64_t clip[0x100 / 8] = {};
    clip[1] = reinterpret_cast<uint64_t>(motion);
    const auto pose = [&] {
        Pose out;
        std::memcpy(out.data(), reinterpret_cast<uint8_t*>(clip) + 0x10, sizeof(out));
        return out;
    };
    const auto current = [&] {
        float time;
        std::memcpy(&time, motion + 0x70, 4);
        return time;
    };
    const auto run = [&](float time) {
        motion_track.calls = 0;
        sample_motion(clip, time);
        return pose();
    };
    assert(run(2) == motion_track.frames[2] && motion_track.calls == 1 && current() == 2); // as shipped
    Pose half = run(2.5f); // inside a shot: between its frames
    assert(std::abs(half[0] - 0.25f) < 1e-6f && half[6] == 10 && current() == 2.5f);
    half = run(0.5f);
    assert(std::abs(half[0] - 0.05f) < 1e-6f && current() == 0.5f);
    assert(run(3) == motion_track.frames[3]);
    assert(run(3.5f) == motion_track.frames[3] && current() == 3.5f); // the cut: the earlier frame stays
    assert(run(4) == motion_track.frames[4] && current() == 4);
    half = run(4.5f); // the new shot moves on between its frames
    assert(std::abs(half[0] - 40.45f) < 1e-4f && half[4] == 30 && current() == 4.5f);
    assert(run(7.5f) == motion_track.frames[7]); // past the end: the last frame
    clip[1] = 0; // not loaded: nothing sampled, nothing touched
    const Pose before = pose();
    sample_motion(clip, 3.5f);
    assert(pose() == before);

    // The view clip: a steady zoom, then another lens at the cut 3 -> 4.
    view_track.offset = 0x50;
    for (int i = 0; i < 8; ++i) {
        Pose lens{};
        lens[4] = i < 4 ? 30 + 0.2f * i : 55;
        view_track.frames.push_back(lens);
    }
    uint8_t view_motion[0x80] = {};
    uint64_t view_clip[0x100 / 8] = {};
    view_clip[1] = reinterpret_cast<uint64_t>(view_motion);
    const auto lens = [&](float time) {
        sample_view(view_clip, time);
        float value;
        std::memcpy(&value, reinterpret_cast<uint8_t*>(view_clip) + 0x50 + 4 * 4, 4);
        return value;
    };
    assert(lens(1) == 30.2f && std::abs(lens(1.5f) - 30.3f) < 1e-5f);
    assert(lens(3.5f) == 30.6f && lens(4) == 55 && lens(4.5f) == 55);
    std::puts("PASS: camera clips are sampled between frames inside a shot and keep the earlier frame across a cut");
}

/// The PT_LOAD bytes of an eboot.elf at their virtual addresses.
bool LoadElf(const char* path, Image& image) {
    std::ifstream file(path, std::ios::binary);
    std::vector<uint8_t> elf{std::istreambuf_iterator<char>(file), {}};
    if (elf.size() < 64 || std::memcmp(elf.data(), "\x7f" "ELF", 4)) return false;
    uint64_t phoff;
    uint16_t phentsize, phnum;
    std::memcpy(&phoff, &elf[0x20], 8);
    std::memcpy(&phentsize, &elf[0x36], 2);
    std::memcpy(&phnum, &elf[0x38], 2);
    for (unsigned i = 0; i < phnum; ++i) {
        const uint8_t* ph = &elf[phoff + i * phentsize];
        uint32_t type;
        uint64_t offset, vaddr, filesz;
        std::memcpy(&type, ph, 4);
        std::memcpy(&offset, ph + 8, 8);
        std::memcpy(&vaddr, ph + 16, 8);
        std::memcpy(&filesz, ph + 32, 8);
        if (type != 1 || vaddr + filesz > ImageSize || offset + filesz > elf.size()) continue;
        std::memcpy(image.data + vaddr, &elf[offset], filesz);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    TestWrites();
    TestStart();
    TestFailClosed();
    TestCamera();
    TestMouthTrack();
    TestSubtitleWindow();
    TestCutRules();
    TestCameraHooks();
    if (argc > 1) {
        Image image;
        assert(LoadElf(argv[1], image));
        std::string problem;
        if (!ValkyriaWidePatches::Matches(image.base(), Writes(), &problem)) {
            std::printf("FAIL: %s: %s\n", argv[1], problem.c_str());
            return 1;
        }
        std::printf("PASS: every patch site matches %s\n", argv[1]);
    }
    return 0;
}
