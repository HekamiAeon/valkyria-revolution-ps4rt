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
    check(3441, 3441, 1080, 1, 760, 761);
    assert(!Fit(3841, 1080, 3841, 1080, {1, 1})); // beyond 32:9
    // Every wide canvas at its own size: equal side areas beside the 16:9 recording.
    for (const auto& size : {std::array{2560u, 1080u}, std::array{3440u, 1440u},
                             std::array{3840u, 1440u}, std::array{3840u, 1080u},
                             std::array{5120u, 1440u}}) {
        const auto layout = Fit(float(size[0]), float(size[1]), size[0], size[1], {1, 1});
        const uint32_t video = size[1] * 16 / 9;
        assert(layout && layout->left == (size[0] - video) / 2 &&
               layout->right == size[0] - video - layout->left);
        assert(std::abs(layout->viewport_scale - float(size[1]) / 1080) < 0.00001f);
    }

    assert(!Fit(1920, 1080, 1920, 1080, {1, 1})); // Original default path.
    assert(!Fit(2560, 1080, 3840, 1080, {1.5, 1}));
    const auto native1440 = Fit(5120, 1440, 5120, 1440, {1, 1});
    assert(native1440 && native1440->left == 1280 && native1440->right == 1280);
    assert(std::abs(native1440->viewport_scale - 4.f / 3) < 0.00001f);
    // Test the actual clip-space mapping: the authored 1920x1080 quad must
    // become 2560x1440 at x1280, including Vulkan's negative viewport height.
    const auto vp = Viewport({0, 1440, 5120, -1440}, float(native1440->left),
                             native1440->viewport_scale);
    const auto pixel = [&](float x, float y) {
        const float ndcx = 2 * x / 5120 - 1;
        const float ndcy = 1 - 2 * y / 1440;
        return std::array<float, 2>{vp[0] + (ndcx + 1) * vp[2] / 2,
                                    vp[1] + (ndcy + 1) * vp[3] / 2};
    };
    const auto top = pixel(0, 0), bottom = pixel(1920, 1080);
    assert(std::abs(top[0] - 1280) < 0.001f && std::abs(top[1]) < 0.001f);
    assert(std::abs(bottom[0] - 3840) < 0.001f && std::abs(bottom[1] - 1440) < 0.001f);
    // The other 16:9 native sizes: the recording fills the canvas (no side areas), scaled by
    // height / 1080. The quad's clip coordinates are 2x/W - 1 (live-pixel transform); from 1080
    // lines up they stay inside the clip volume and the scaled viewport places them, below that
    // the quad would be clipped to a crop, so its positions are scaled and the viewport kept.
    for (const auto& size : {std::array{1024u, 576u}, std::array{1280u, 720u},
                             std::array{2560u, 1440u}, std::array{3840u, 2160u}}) {
        const float w = float(size[0]), h = float(size[1]);
        const auto layout = Fit(w, h, size[0], size[1], {1, 1});
        assert(layout && layout->left == 0 && layout->right == 0 && layout->width == size[0]);
        assert(std::abs(layout->viewport_scale - h / 1080) < 0.00001f);
        const auto shrink = ShrinkPositions(w, h);
        assert(bool(shrink) == (h < 1080));
        const float scale = shrink ? 1 : layout->viewport_scale;
        const auto v = Viewport({0, h, w, -h}, 0, scale);
        const auto at = [&](float x, float y) {
            if (shrink) x = x * shrink->scale + shrink->left, y *= shrink->scale;
            const float ndcx = 2 * x / w - 1, ndcy = 1 - 2 * y / h;
            assert(ndcx >= -1.0001f && ndcx <= 1.0001f && ndcy >= -1.0001f && ndcy <= 1.0001f);
            return std::array<float, 2>{v[0] + (ndcx + 1) * v[2] / 2, v[1] + (ndcy + 1) * v[3] / 2};
        };
        const auto a = at(0, 0), b = at(1920, 1080);
        assert(std::abs(a[0]) < 0.001f && std::abs(a[1]) < 0.001f);
        assert(std::abs(b[0] - w) < 0.01f && std::abs(b[1] - h) < 0.01f);
    }
    assert(!ShrinkPositions(1920, 1080) && !ShrinkPositions(2560, 1080) &&
           !ShrinkPositions(5120, 1440) && !ShrinkPositions(1600, 900));
    assert(!Fit(1600, 900, 1600, 900, {1, 1})); // not a native canvas
    assert(!Fit(5120, 1440, 5120, 1080, {1, 1}));
    assert(!Fit(5120, 1200, 5120, 1200, {1, 1}));
    assert(!Fit(3840, 1080, 0, 0, {0, 0}));
    assert(!Fit(std::numeric_limits<float>::infinity(), 1080, 3840, 1080, {1, 1}));
    std::puts("PASS: movie placement, proportional side areas, small/invalid video rejection, "
              "uniform scaling, 16:9 native sizes filled inside the clip volume (positions below 1080 "
              "lines) and the stock 1920x1080 path unchanged");
}
