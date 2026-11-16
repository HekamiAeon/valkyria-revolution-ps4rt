// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

#include "valkyria_wide_canvas.h"

namespace Vulkan::ValkyriaUi {

struct Layout {
    float x_offset;
    float scale;
};

// History-book pages draw into stock 960x1080 textures as off-screen UI layers.
// The native canvas leaves off-screen layers untranslated (as the mission-table
// map layer needs), but the GUI root still scales them for the native height.
// Undo that scale for these targets without changing vertices or cached glyphs.
inline std::optional<Layout> BookPage(float native_width, float native_height,
                                      uint32_t width, uint32_t height,
                                      std::array<float, 2> target_scale,
                                      std::span<const float> constants) {
    if (width != 960 || height != 1080 || constants.size() < 56 ||
        !ValkyriaWide::Native(native_width, native_height) ||
        target_scale[0] != 1 || target_scale[1] != 1) return {};
    const auto close = [](float a, float b) {
        return std::isfinite(a) && std::abs(a - b) < 0.0000001f;
    };
    const float gui_scale = native_height / 1080.f;
    // Projection and transposed world matrix, verified in the page captures. Page
    // text is placed by the world translation (observed 130,226 authored units,
    // stored GUI-scaled); undoing the root scale returns it to its page position.
    if (!close(constants[4], 2.f / native_width) ||
        !close(constants[9], -2.f / native_height) ||
        !close(constants[44], gui_scale) || !close(constants[49], gui_scale) ||
        !close(constants[54], 1) || !std::isfinite(constants[47]) ||
        !std::isfinite(constants[51])) return {};
    return Layout{0.f, 1.f / gui_scale};
}

// Full-screen quads the game sizes from the live screen in pixels but draws under the GUI
// root, which scales by s = height / 1080 (the screen fades of FUN_0020b8d0 and similar
// filter layers, or a stock-pixel copy the canvas UI widened to the screen). The stock game
// only ran at s >= 1, where such a quad overruns the screen unseen; below 1080 lines it
// covers only the top-left s^2 of it, and above, a textured one shows a zoomed crop. The
// draw's own transform proves the mismatch: a top-left pixel projection (2/W, -2/H), an
// unrotated, untranslated world scaled by exactly s, and vertices on the screen's pixel
// corners. Returns s; positions divided by it cover the screen exactly.
inline std::optional<float> PixelScreenQuadScale(float native_width, float native_height,
                                                 std::span<const float> constants) {
    if (constants.size() < 56 || !ValkyriaWide::Native(native_width, native_height)) return {};
    const float s = native_height / 1080.f;
    if (std::abs(s - 1) < 0.0001f) return {}; // the GUI root is the identity
    const auto close = [](float a, float b) {
        return std::isfinite(a) && std::abs(a - b) <= 0.00001f * std::max(1.f, std::abs(b));
    };
    const auto& c = constants;
    if (!close(c[4], 2.f / native_width) || !close(c[7], -1) ||
        !close(c[9], -2.f / native_height) || !close(c[11], 1) ||
        !close(c[44], s) || !close(c[49], s) || !close(c[54], 1)) return {};
    for (unsigned i : {45u, 46u, 47u, 48u, 50u, 51u, 52u, 53u})
        if (!close(c[i], 0)) return {};
    return s;
}

// Every point on a screen pixel corner, all four corners present.
inline bool PixelScreenQuad(float native_width, float native_height,
                            std::span<const std::array<float, 2>> points) {
    if (points.size() < 4) return false;
    unsigned corners = 0;
    for (const auto& p : points) {
        const bool x0 = std::abs(p[0]) <= 0.01f, x1 = std::abs(p[0] - native_width) <= 0.01f;
        const bool y0 = std::abs(p[1]) <= 0.01f, y1 = std::abs(p[1] - native_height) <= 0.01f;
        if ((!x0 && !x1) || (!y0 && !y1)) return false;
        corners |= 1u << (unsigned(x1) + 2 * unsigned(y1));
    }
    return corners == 15;
}

} // namespace Vulkan::ValkyriaUi
