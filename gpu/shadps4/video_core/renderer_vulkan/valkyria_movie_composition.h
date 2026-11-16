// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

#include "valkyria_wide_canvas.h"

namespace Vulkan::ValkyriaMovie {

// The recorded fullscreen movie path submits these native pixel coordinates.
// Colour and UV fields are deliberately outside this position-only check.
inline bool FullscreenQuad(std::span<const std::array<float, 2>> points) {
    constexpr std::array<std::array<float, 2>, 4> expected{{
        {0, 0}, {1920, 0}, {0, 1080}, {1920, 1080},
    }};
    if (points.size() != expected.size()) return false;
    for (size_t i = 0; i < points.size(); ++i) {
        for (size_t c = 0; c < 2; ++c) {
            if (!std::isfinite(points[i][c]) ||
                std::abs(points[i][c] - expected[i][c]) > 0.01f) return false;
        }
    }
    return true;
}

struct Layout {
    uint32_t left;
    uint32_t right;
    uint32_t width;
    uint32_t height;
    float viewport_scale;
};

inline bool UntranslatedTransform(std::span<const float> data, float xscale, float yscale) {
    if (data.size() < 56 || !std::isfinite(xscale) || !std::isfinite(yscale) ||
        xscale == 0 || yscale == 0) return false;
    const auto close = [](float a, float b) {
        return std::isfinite(a) && std::abs(a - b) < 0.00001f;
    };
    // Native queued renderer290200 writes the transposed world matrix at0xb0.
    return close(data[4], 1.f / xscale) && close(data[9], 1.f / yscale) &&
        close(data[44], 1) && close(data[49], 1) && close(data[54], 1) &&
        close(data[47], 0) && close(data[51], 0);
}

// Authored 1920x1080 recordings on a native canvas (valkyria_wide_canvas.h): scaled by
// height / 1080, centred between black side areas on a wide canvas and filling a 16:9 one
// (left = right = 0). The movie quad is in live pixels, so without this it covers only
// 1920x1080 of a larger canvas and a crop of itself on a smaller one. Host target scaling
// and authored-video scaling are separate.
inline std::optional<Layout> Fit(float native_width, float native_height,
                                 uint32_t width, uint32_t height,
                                 std::array<float, 2> scale) {
    if (!std::isfinite(native_width) || !std::isfinite(native_height) ||
        !std::isfinite(scale[0]) || !std::isfinite(scale[1]) ||
        !ValkyriaWide::Native(native_width, native_height) ||
        scale[0] <= 0 || scale[1] <= 0 || std::abs(scale[0] - scale[1]) > 0.0001f ||
        std::abs(native_width * scale[0] - float(width)) > 0.5f ||
        std::abs(native_height * scale[1] - float(height)) > 0.5f) return {};
    const auto video_width = uint32_t(std::lround(1920.0 * double(height) / 1080.0));
    if (video_width > width) return {};
    const uint32_t left = (width - video_width) / 2;
    return Layout{left, width - video_width - left, width, height, native_height / 1080.f};
}

// Preserve negative-height Vulkan viewports. Scaling their origin as well as
// their extent keeps the top edge at zero and expands the recording uniformly.
// A canvas shorter than the recording (below 1080 lines): the movie quad scaled through the
// viewport would extend past the clip volume and show a crop, so a copy of its positions is
// scaled to the canvas instead, x' = scale * x + left and y' = scale * y in canvas pixels.
struct Positions {
    float scale;
    float left;
};

inline std::optional<Positions> ShrinkPositions(float native_width, float native_height) {
    if (!ValkyriaWide::Native(native_width, native_height) || native_height >= 1080) return {};
    const float scale = native_height / 1080.f;
    return Positions{scale, (native_width - 1920.f * scale) / 2};
}

inline std::array<float, 4> Viewport(std::array<float, 4> viewport, float left, float scale) {
    return {viewport[0] * scale + left, viewport[1] * scale,
            viewport[2] * scale, viewport[3] * scale};
}

} // namespace Vulkan::ValkyriaMovie
