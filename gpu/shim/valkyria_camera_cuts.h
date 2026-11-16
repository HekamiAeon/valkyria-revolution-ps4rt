// SPDX-License-Identifier: GPL-2.0-or-later
// Valkyria Revolution (CUSA06810) at 60 updates a second (valkyria_frame_rate.h): camera cuts
// inside keyframed camera animations. A camera motion is sampled at a time in authored frames
// (FUN_0075eee0: linear between the frames around it). The shipped game only asks for whole
// frames, so a cut is two consecutive frames in different places. At 60 every second update
// asks for the half frame between them, and the camera would show the midpoint of two shots
// for one update. Here: when the step between the two whole frames around the time is a cut,
// the earlier frame stays until the later one is reached, as it does at 30. Everything else is
// sampled at the time asked for.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace ValkyriaCameraCuts {

/// The floats a camera clip is sampled into (the game's buffers hold sixteen).
inline constexpr int PoseSize = 16;

/// A one-frame step that stands out from the motion around it: more than four times the
/// larger of the steps before and after, plus `floor` (the smallest step worth calling a cut:
/// the midpoint of two nearly equal views is not visible, and stored values are quantised).
constexpr bool StandsOut(float step, float before, float after, float floor) {
    return step > 4.0f * std::max(before, after) + floor;
}

inline float Distance(const float* a, const float* b) {
    return std::sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) +
                     (a[2] - b[2]) * (a[2] - b[2]));
}

/// The position, target and roll clip (FUN_00359360: position at [0..2], target at [4..6],
/// roll in radians at [14]): the step from whole frame `a` to `b` is a cut. `p` is the frame
/// before `a`, `q` the frame after `b`. Position and target are measured against the distance
/// the camera looks over.
inline bool MotionCut(const float* p, const float* a, const float* b, const float* q) {
    const float floor = 0.05f * std::min(Distance(a, a + 4), Distance(b, b + 4));
    return StandsOut(Distance(a, b), Distance(p, a), Distance(b, q), floor) ||
           StandsOut(Distance(a + 4, b + 4), Distance(p + 4, a + 4), Distance(b + 4, q + 4), floor) ||
           StandsOut(std::abs(b[14] - a[14]), std::abs(a[14] - p[14]), std::abs(q[14] - b[14]), 0.05f);
}

/// The view clip (FUN_0035a270: field of view and the other lens values, each on its own):
/// a value steps by more than a twentieth of itself in one frame, out of line with the frames
/// around it.
inline bool ViewCut(const float* p, const float* a, const float* b, const float* q) {
    for (int i = 0; i < PoseSize; ++i) {
        const float floor = 0.05f * std::max(std::abs(a[i]), std::abs(b[i])) + 0.01f;
        if (StandsOut(std::abs(b[i] - a[i]), std::abs(a[i] - p[i]), std::abs(q[i] - b[i]), floor)) {
            return true;
        }
    }
    return false;
}

/// The two clip samplers the hooks replace, and the bytes each must start with.
inline constexpr uint64_t MotionSampler = 0x359360, ViewSampler = 0x35a270;
inline constexpr uint8_t SamplerEntry[16] = {0x55, 0x48, 0x89, 0xe5,  // push rbp ; mov rbp,rsp
                                             0x53, 0x50,              // push rbx ; push rax
                                             0x48, 0x89, 0xfb,        // mov rbx,rdi
                                             0xc5, 0xf8, 0x28, 0xd0,  // vmovaps xmm2,xmm0
                                             0x48, 0x8b, 0x3b};       // mov rdi,[rbx]

/// Builds the way back into both samplers for the game image at base (their entry bytes must
/// already be verified). False changes nothing and names the reason.
bool Prepare(uintptr_t base, std::string* problem);
/// Routes both samplers through the hooks (after Prepare, while the game's code is writable).
void Install(uintptr_t base);

} // namespace ValkyriaCameraCuts
