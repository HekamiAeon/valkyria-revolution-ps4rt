// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <algorithm>
#include <cstdint>

// A scene the game draws into a texture of its own and shows as a 2D layer (Valkyria's talk
// portrait): its jittered frames are gathered into an output-size copy by
// host_shaders/scene_layer.comp. The rules shared with the tests.
namespace Vulkan::SceneLayer {

/// Weight of the newest frame's texel at an output pixel's centre, `age` frames after the
/// copy began: it is built from its first frames, then holds steady against the jitter
/// sequence (a larger steady weight flickers along edges, a smaller one trails animation).
constexpr float NewestWeight(uint32_t age) {
    return std::max(0.1f, 1.0f / (1.0f + 0.5f * float(age)));
}

/// One side of the output-size copy of a layer `layer` texels long, drawn in a scene of
/// `render` pixels that is shown on `output` pixels.
constexpr uint32_t OutputExtent(uint32_t layer, uint32_t render, uint32_t output) {
    return render ? uint32_t((uint64_t(layer) * output + render / 2) / render) : 0;
}

/// Whether a frame whose scene is not the upscaled frame may be drawn with jitter: when that
/// scene was a single layer and it was gathered. Any other layer is shown as the game drew
/// it, and jitter would shake it.
constexpr bool KeepsJitter(uint32_t layers_drawn, bool gathered) {
    return layers_drawn == 1 && gathered;
}

} // namespace Vulkan::SceneLayer
