// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <cmath>

namespace Vulkan::ValkyriaWide {

// Native wide canvases: the game's own GUI heights (1080, and 1440 for its
// 1440 UI) and an integer width beyond 16:9 up to 32:9, such as 2560x1080, 3440x1440,
// 3840x1440, 3840x1080 and 5120x1440. Every layout correction derives from this size.
inline bool Canvas(float width, float height) {
    return (height == 1080.f || height == 1440.f) && width > 0 && width <= 16384.f &&
           width == std::floor(width) && width * 9 > height * 16 && width * 9 <= height * 32;
}

// The other 16:9 internal resolutions (1024x576, 1280x720, 2560x1440, 3840x2160): no side
// margins, but the GUI root still scales by height / 1080.
inline bool Standard(float width, float height) {
    return (height == 576.f || height == 720.f || height == 1440.f || height == 2160.f) &&
           width * 9 == height * 16;
}

// Any native canvas other than the game's own 1920x1080: its size-dependent fixes apply.
inline bool Native(float width, float height) {
    return Canvas(width, height) || Standard(width, height);
}

} // namespace Vulkan::ValkyriaWide
