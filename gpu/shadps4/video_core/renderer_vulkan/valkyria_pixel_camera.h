// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>

#include "valkyria_wide_canvas.h"

namespace Vulkan::ValkyriaPixelCamera {

// The engine's pixel camera: an unrotated view placed proj_y * height / 2 in front of its
// plane, so one unit there is one pixel of the live height. Screen effects (the paper fog)
// author that plane in stock 1920x1080 pixels, and any other effect drawn through the same
// camera inherits the same 16:9 box on a native canvas. The common 3D constant block keeps
// the projection at 4..19, the view (twice) at 20..31 and 32..43, and the world at 44..55.
struct Transform {
    float kx, ky, kz, ox, oy; // x' = kx * x + ox, y' = ky * y + oy, z' = kz * z
};

// Screen extent of the drawn vertices in stock pixels from the centre (world applied).
struct Extent {
    float min_x, max_x, max_abs_z;
};

inline bool Camera(float width, float height, std::span<const float> c) {
    if (!ValkyriaWide::Native(width, height) || c.size() < 56) return false;
    const auto close = [](float a, float b, float epsilon = 0.0001f) {
        return std::abs(a - b) <= epsilon; // false for NaN
    };
    // Cheapest rejection first: every other draw runs this test.
    const float distance = c[9] * height / 2;
    if (!(c[9] > 0) || !close(c[31], distance, .02f) || !close(c[43], distance, .02f) ||
        !close(c[4] * width, c[9] * height, .02f)) return false;
    for (unsigned i = 4; i < 56; ++i) if (!std::isfinite(c[i])) return false;
    constexpr float view[11] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    for (unsigned i = 0; i < 11; ++i)
        if (!close(c[20 + i], view[i]) || !close(c[32 + i], view[i])) return false;
    // Unrotated world with a positive scale, its origin on the camera plane.
    for (unsigned i : {45u, 46u, 48u, 50u, 52u, 53u}) if (!close(c[i], 0)) return false;
    return c[44] > 0 && c[49] > 0 && c[54] > 0 && close(c[55], -distance, .02f);
}

// Vertices flagged in `used`, float x, y and optionally z at the start of each stride.
inline std::optional<Extent> Measure(const std::byte* vertices, uint32_t stride,
                                     uint32_t components, std::span<const uint8_t> used,
                                     std::span<const float> c) {
    if (components < 2 || components > 4 || stride < components * 4 || c.size() < 56)
        return {};
    Extent extent{INFINITY, -INFINITY, 0};
    bool any = false;
    for (size_t i = 0; i < used.size(); ++i) {
        if (!used[i]) continue;
        float p[3]{};
        std::memcpy(p, vertices + i * stride, std::min(components, 3u) * sizeof(float));
        for (float f : p) if (!std::isfinite(f)) return {};
        const float x = c[44] * p[0] + c[47];
        extent.min_x = std::min(extent.min_x, x);
        extent.max_x = std::max(extent.max_x, x);
        extent.max_abs_z = std::max(extent.max_abs_z, std::abs(p[2]));
        any = true;
    }
    return any ? std::optional(extent) : std::nullopt;
}

// A flat plane covering the full stock width (the paper fog) spans the native width; any
// other pixel-camera content keeps its exact stock framing, in the centred 16:9 area.
inline std::optional<Transform> Fit(float width, float height, std::span<const float> c,
                                    const Extent& extent) {
    if (!Camera(width, height, c)) return {};
    const float s = height / 1080.f;
    const bool sheet = extent.max_abs_z <= 0.001f && extent.min_x <= -959.5f &&
                       extent.max_x >= 959.5f;
    const float kx = sheet ? width / 1920.f : s;
    const Transform t{kx, s, sheet ? 1.f : s, c[47] * (kx - 1) / c[44],
                      c[51] * (s - 1) / c[49]};
    if (t.kx == 1 && t.ky == 1 && t.kz == 1) return {};
    return t;
}

inline void Apply(std::byte* vertices, uint32_t stride, uint32_t components,
                  std::span<const uint8_t> used, const Transform& t) {
    for (size_t i = 0; i < used.size(); ++i) {
        if (!used[i]) continue;
        float p[3]{};
        std::byte* v = vertices + i * stride;
        const uint32_t n = std::min(components, 3u);
        std::memcpy(p, v, n * sizeof(float));
        p[0] = t.kx * p[0] + t.ox;
        p[1] = t.ky * p[1] + t.oy;
        p[2] = t.kz * p[2];
        std::memcpy(v, p, n * sizeof(float));
    }
}

} // namespace Vulkan::ValkyriaPixelCamera
