// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution camera cuts at 60 updates a second (valkyria_camera_cuts.h): hooks on
// the two camera clip samplers, FUN_00359360(clip, time) and FUN_0035a270(clip, time).
#include "valkyria_camera_cuts.h"

#include <sys/mman.h>
#include <unistd.h>

#include <cstring>

namespace ValkyriaCameraCuts {
namespace {

using Sampler = void (*)(uint64_t* clip, float time);
using Cut = bool (*)(const float*, const float*, const float*, const float*);

/// The samplers as shipped: their first instructions, then a jump back into the game.
Sampler original_motion, original_view;

/// jmp [rip+0] ; target
void Jump(uint8_t* at, uintptr_t target) {
    const uint8_t code[6] = {0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(at, code, sizeof(code));
    std::memcpy(at + sizeof(code), &target, sizeof(target));
}
constexpr size_t JumpSize = 14;

/// Samples `clip` at `time`. At a time between two whole frames whose step is a cut, the pose
/// is the earlier frame's; the clip's current time stays the one asked for.
void Sample(uint64_t* clip, float time, Sampler original, size_t pose_offset, Cut cut) {
    const float whole = std::floor(time);
    if (!(time > whole)) { // a whole frame (every shipped call), or not a number
        original(clip, time);
        return;
    }
    float frames[4][PoseSize];
    for (int i = 0; i < 4; ++i) {
        original(clip, whole + static_cast<float>(i - 1));
        std::memcpy(frames[i], reinterpret_cast<const uint8_t*>(clip) + pose_offset, sizeof(frames[i]));
    }
    original(clip, time);
    if (!cut(frames[0], frames[1], frames[2], frames[3])) return;
    // clip[1] is the loaded motion; it keeps the time it was last sampled at (+0x70).
    uint8_t* const motion = reinterpret_cast<uint8_t*>(clip[1]);
    float current = 0;
    if (motion) std::memcpy(&current, motion + 0x70, sizeof(current));
    original(clip, whole);
    if (motion) std::memcpy(motion + 0x70, &current, sizeof(current));
}

void SampleMotion(uint64_t* clip, float time) {
    Sample(clip, time, original_motion, 0x10, MotionCut);
}
void SampleView(uint64_t* clip, float time) {
    Sample(clip, time, original_view, 0x50, ViewCut);
}

} // namespace

bool Prepare(uintptr_t base, std::string* problem) {
    const size_t page = static_cast<size_t>(sysconf(_SC_PAGESIZE));
    void* const memory = mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        if (problem) *problem = "no memory for the camera hooks";
        return false;
    }
    uint8_t* const code = static_cast<uint8_t*>(memory);
    const uint64_t samplers[2] = {MotionSampler, ViewSampler};
    for (int i = 0; i < 2; ++i) {
        uint8_t* const back = code + 64 * i;
        std::memcpy(back, SamplerEntry, sizeof(SamplerEntry));
        Jump(back + sizeof(SamplerEntry), base + samplers[i] + sizeof(SamplerEntry));
    }
    if (mprotect(memory, page, PROT_READ | PROT_EXEC)) {
        munmap(memory, page);
        if (problem) *problem = "the camera hooks could not be made executable";
        return false;
    }
    original_motion = reinterpret_cast<Sampler>(code);
    original_view = reinterpret_cast<Sampler>(code + 64);
    return true;
}

void Install(uintptr_t base) {
    const std::pair<uint64_t, Sampler> hooks[2] = {{MotionSampler, SampleMotion}, {ViewSampler, SampleView}};
    for (const auto& [at, hook] : hooks) {
        uint8_t entry[sizeof(SamplerEntry)];
        std::memset(entry, 0x90, sizeof(entry));
        static_assert(JumpSize <= sizeof(entry));
        Jump(entry, reinterpret_cast<uintptr_t>(hook));
        std::memcpy(reinterpret_cast<void*>(base + at), entry, sizeof(entry));
    }
}

} // namespace ValkyriaCameraCuts
