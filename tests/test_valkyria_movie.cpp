// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu/shadps4/video_core/renderer_vulkan/valkyria_movie_composition.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>

using namespace Vulkan::ValkyriaMovie;

int main() {
    std::array<std::array<float, 2>, 4> points{{
        {0, 0}, {1920, 0}, {0, 1080}, {1920, 1080},
    }};
    assert(FullscreenQuad(points));
    points[1][0] = 320; // Smaller interface videos must retain their own placement.
    assert(!FullscreenQuad(points));
    points[1][0] = 1920;
    points[0][0] = std::numeric_limits<float>::quiet_NaN();
    assert(!FullscreenQuad(points));
    assert(!FullscreenQuad(std::span(points).first(3)));

    // Synthetic projection + world3x4 at the native renderer's observed0xb0 offset.
    std::array<float, 60> constants{};
    constants[4] = 2.f / 3840;
    constants[9] = -2.f / 1080;
    constexpr std::array<float, 12> world{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, -10};
    std::copy(world.begin(), world.end(), constants.begin() + 44);
    assert(UntranslatedTransform(constants, 1920, -540));
    constants[47] = 100; // Positioned interface movies do not match the fullscreen path.
    assert(!UntranslatedTransform(constants, 1920, -540));
    constants[47] = 0;
    assert(!UntranslatedTransform(std::span(constants).first(55), 1920, -540));
    assert(!UntranslatedTransform(constants, 960, -540));

    const auto check = [](float native_width, uint32_t width, uint32_t height,
                          float scale, uint32_t left, uint32_t right) {
        const auto layout = Fit(native_width, 1080, width, height, {scale, scale});
        assert(layout && layout->left == left && layout->right == right);
        // Side clears leave exactly the proportional video region untouched.
        const double video_width = double(width - layout->left - layout->right);
        assert(std::abs(video_width / double(height) - 16.0 / 9.0) < 0.001);
        assert(std::abs(double(layout->left) - double(layout->right)) <= 1);
        assert(layout->width == width && layout->height == height);
    };
    check(3840, 3840, 1080, 1, 960, 960);
    check(2560, 2560, 1080, 1, 320, 320);
    check(3440, 3440, 1080, 1, 760, 760);
    check(3840, 5120, 1440, 4.f / 3, 1280, 1280);
    check(3841, 3841, 1080, 1, 960, 961);

    assert(!Fit(1920, 1080, 1920, 1080, {1, 1})); // Original default path.
    assert(!Fit(2560, 1080, 3840, 1080, {1.5, 1}));
    assert(!Fit(5120, 1440, 5120, 1440, {1, 1})); // Unverified internal height.
    assert(!Fit(3840, 1080, 0, 0, {0, 0}));
    assert(!Fit(std::numeric_limits<float>::infinity(), 1080, 3840, 1080, {1, 1}));
    std::puts("PASS: movie placement, proportional side areas, small/invalid video rejection, "
              "uniform scaling and unchanged 16:9 fallback");
}
