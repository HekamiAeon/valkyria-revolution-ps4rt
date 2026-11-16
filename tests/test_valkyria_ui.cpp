// SPDX-License-Identifier: GPL-2.0-or-later
#include "gpu/shadps4/video_core/renderer_vulkan/valkyria_ui_composition.h"
#include "gpu/shadps4/video_core/renderer_vulkan/valkyria_movie_composition.h"
#include "gpu/shadps4/video_core/renderer_vulkan/valkyria_pixel_camera.h"
#include "gpu/shim/ps4rt_graphics.h"

#include <bit>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>

// Wide canvases of both GUI heights, from just past 16:9 to 32:9.
constexpr std::array<std::array<float, 2>, 5> kCanvases{{
    {2560, 1080}, {3440, 1440}, {3840, 1440}, {3840, 1080}, {5120, 1440},
}};
// Every native canvas: the wide ones and the other 16:9 internal resolutions.
constexpr std::array<std::array<float, 2>, 9> kNative{{
    {2560, 1080}, {3440, 1440}, {3840, 1440}, {3840, 1080}, {5120, 1440},
    {1024, 576}, {1280, 720}, {2560, 1440}, {3840, 2160},
}};

int main() {
    using Vulkan::ValkyriaWide::Canvas;
    for (const auto& [width, height] : kCanvases) assert(Canvas(width, height));
    for (const auto& size : {std::array{1920.f, 1080.f}, std::array{2560.f, 1440.f},
                             std::array{3840.f, 2160.f}, std::array{3840.f, 1600.f},
                             std::array{5760.f, 1080.f}, std::array{3440.5f, 1440.f},
                             std::array{0.f, 1080.f}, std::array{-3440.f, 1440.f},
                             std::array{std::numeric_limits<float>::quiet_NaN(), 1440.f},
                             std::array{std::numeric_limits<float>::infinity(), 1440.f}})
        assert(!Canvas(size[0], size[1]));
    // The menu's internal resolutions: the game's own 1080p, then native canvases (16:9 first).
    using Vulkan::ValkyriaWide::Native;
    using Vulkan::ValkyriaWide::Standard;
    namespace G = Ps4RtGraphics;
    assert(!Native(float(G::ResolutionWidths[0]), float(G::ResolutionHeights[0])));
    for (size_t i = 1; i < G::Resolutions.size(); ++i) {
        const float w = float(G::ResolutionWidths[i]), h = float(G::ResolutionHeights[i]);
        assert(Native(w, h) && Canvas(w, h) == (i >= 5) && Standard(w, h) == (i < 5));
    }
    for (const auto& size : {std::array{1920.f, 1080.f}, std::array{1600.f, 900.f},
                             std::array{5120.f, 2880.f}, std::array{2560.f, 1600.f}})
        assert(!Native(size[0], size[1]));
    std::puts("PASS: wide canvases 2560x1080..5120x1440 at both GUI heights; 16:9 576/720/1440/"
              "2160 native canvases (every menu resolution past 1080p); 1080p, other 16:9 sizes, "
              "other heights, beyond 32:9 and fractional/invalid sizes excluded");

    using Vulkan::ValkyriaUi::BookPage;
    for (const auto& [width, height] : kNative) {
        const float s = height / 1080;
        std::array<float, 56> c{};
        c[4] = 2 / width; c[9] = -2 / height;
        c[44] = c[49] = s; c[54] = 1;
        const auto fix = BookPage(width, height, 960, 1080, {1, 1}, c);
        assert(fix);
        const auto vp = Vulkan::ValkyriaMovie::Viewport({0, height, width, -height},
                                                        fix->x_offset, fix->scale);
        // Off-screen layers keep their authored vertices (no native canvas
        // translation); the GUI shader still scales them. Both page edges must
        // return to stock pixel positions.
        for (const auto& xy : {std::array{0.f, 0.f}, std::array{960.f, 1080.f},
                               std::array{245.f, 512.f}}) {
            const float clip_x = 2 * (xy[0] * s) / width - 1;
            const float clip_y = 1 - 2 * xy[1] * s / height;
            const float x = vp[0] + (clip_x + 1) * vp[2] / 2;
            const float y = vp[1] + (clip_y + 1) * vp[3] / 2;
            assert(std::abs(x - xy[0]) < 0.001f && std::abs(y - xy[1]) < 0.001f);
        }
        assert(!BookPage(width, height, uint32_t(width), uint32_t(height), {1, 1}, c));
        assert(!BookPage(width, height, 960, 1080, {2, 2}, c));
        assert(!BookPage(width, height, 960, 1080, {1, 1}, std::span(c).first(55)));
        // Page text: the world translation (GUI-scaled) places each text block.
        c[47] = 130 * s; c[51] = 226 * s;
        const auto text = BookPage(width, height, 960, 1080, {1, 1}, c);
        assert(text && text->x_offset == fix->x_offset && text->scale == fix->scale);
        for (const auto& xy : {std::array{0.f, 0.f}, std::array{600.f, 700.f}}) {
            const float clip_x = 2 * ((xy[0] * s) + c[47]) / width - 1;
            const float clip_y = 1 - 2 * ((xy[1] * s) + c[51]) / height;
            const float x = vp[0] + (clip_x + 1) * vp[2] / 2;
            const float y = vp[1] + (clip_y + 1) * vp[3] / 2;
            assert(std::abs(x - (xy[0] + 130)) < 0.01f && std::abs(y - (xy[1] + 226)) < 0.01f);
        }
        c[51] = std::numeric_limits<float>::infinity();
        assert(!BookPage(width, height, 960, 1080, {1, 1}, c));
        c[47] = c[51] = 0; c[44] = s * 1.5f;
        assert(!BookPage(width, height, 960, 1080, {1, 1}, c));
        c[44] = std::numeric_limits<float>::quiet_NaN();
        assert(!BookPage(width, height, 960, 1080, {1, 1}, c));
    }
    std::array<float, 56> stock{};
    stock[4] = 2.f / 1920; stock[9] = -2.f / 1080;
    stock[44] = stock[49] = stock[54] = 1;
    assert(!BookPage(1920, 1080, 960, 1080, {1, 1}, stock));
    // Otherwise consistent constants on an unsupported GUI height.
    std::array<float, 56> tall{};
    tall[4] = 2.f / 3840; tall[9] = -2.f / 1600;
    tall[44] = tall[49] = 1600.f / 1080; tall[54] = 1;
    assert(!BookPage(3840, 1600, 960, 1080, {1, 1}, tall));
    std::puts("PASS: book-page pixel layout on every native canvas; full display, other transforms, "
              "invalid inputs, unsupported sizes and default16:9 excluded");

    // Full-screen quads in screen pixels under the GUI root (constants as in the 1024x576 UI
    // pass captures: projection 2/W, -1 | -2/H, 1; world diag s, s, 1; z translation kept).
    using Vulkan::ValkyriaUi::PixelScreenQuad;
    using Vulkan::ValkyriaUi::PixelScreenQuadScale;
    for (const auto& [width, height] : kNative) {
        const float s = height / 1080;
        std::array<float, 56> c{};
        c[4] = 2 / width; c[7] = -1; c[9] = -2 / height; c[11] = 1;
        c[44] = c[49] = s; c[54] = 1; c[55] = -10;
        const auto scale = PixelScreenQuadScale(width, height, c);
        assert(bool(scale) == (height != 1080)); // the GUI root is the identity at 1080
        const std::array<std::array<float, 2>, 4> quad{{{0, 0}, {width, 0}, {0, height}, {width, height}}};
        assert(PixelScreenQuad(width, height, quad));
        if (scale) {
            assert(*scale == s);
            // Divided by the scale, the quad lands exactly on the screen corners.
            for (const auto& p : quad) {
                const float clip_x = c[4] * (c[44] * (p[0] / *scale) + c[47]) + c[7];
                const float clip_y = c[9] * (c[49] * (p[1] / *scale) + c[51]) + c[11];
                const float x = (clip_x + 1) * width / 2, y = (1 - clip_y) * height / 2;
                assert(std::abs(x - p[0]) < 0.01f && std::abs(y - p[1]) < 0.01f);
            }
        }
        // Not the mismatch: no GUI scale (the scene copy), a translated, rotated or otherwise
        // scaled world, another projection, short constants.
        auto other = c; other[44] = other[49] = 1;
        assert(!PixelScreenQuadScale(width, height, other));
        other = c; other[47] = 960 * s; assert(!PixelScreenQuadScale(width, height, other));
        other = c; other[45] = 0.5f; assert(!PixelScreenQuadScale(width, height, other));
        other = c; other[44] = s * 1.5f; assert(!PixelScreenQuadScale(width, height, other));
        other = c; other[4] = 2 / 1920.f; assert(width == 1920 || !PixelScreenQuadScale(width, height, other));
        other = c; other[9] = std::numeric_limits<float>::quiet_NaN();
        assert(!PixelScreenQuadScale(width, height, other));
        assert(!PixelScreenQuadScale(width, height, std::span(c).first(55)));
        // Not a pixel screen quad: authored 1920x1080 (unless that is the screen), a partial
        // rectangle, a missing corner, too few points.
        const std::array<std::array<float, 2>, 4> authored{{{0, 0}, {1920, 0}, {0, 1080}, {1920, 1080}}};
        assert(PixelScreenQuad(width, height, authored) == (width == 1920 && height == 1080));
        const std::array<std::array<float, 2>, 4> partial{{{0, 0}, {width / 2, 0}, {0, height}, {width / 2, height}}};
        assert(!PixelScreenQuad(width, height, partial));
        const std::array<std::array<float, 2>, 4> corner{{{0, 0}, {width, 0}, {0, height}, {0, height}}};
        assert(!PixelScreenQuad(width, height, corner));
        assert(!PixelScreenQuad(width, height, std::span(quad).first(3)));
    }
    assert(!PixelScreenQuadScale(1920, 1080, stock) && !PixelScreenQuadScale(3840, 1600, tall));
    std::puts("PASS: screen-pixel quads under a GUI scale on every native canvas off 1080 lines "
              "(divided back onto the screen corners); unscaled, translated, rotated, other "
              "transforms, partial/authored quads, invalid input and 1080 lines excluded");

    using namespace Vulkan::ValkyriaPixelCamera;
    // Captured paper-fog mesh: 25 vertices of 16 floats (position, normal, colour, alpha, UV).
    using Vertex = std::array<float, 16>;
    std::array<Vertex, 32> fog{};
    for (unsigned i = 0; i < 25; ++i) {
        auto& v = fog[i];
        v[0] = -960.f + (i % 5) * 480.f; v[1] = -480.f + (i / 5) * 240.f;
        v[5] = v[6] = v[7] = v[8] = 1;
        v[9] = 1.f - (i / 5) / 4.f; // Existing fog opacity varies across the grid.
        v[10] = (i % 5) * .7435f - .987f; v[11] = (i / 5) * .37175f;
        v[12] = 1;
    }
    // Vertices this draw does not reference are neither measured nor moved.
    fog[25][0] = std::numeric_limits<float>::quiet_NaN(); fog[26][2] = 50; fog[27][0] = 5000;
    std::array<uint8_t, 32> used{};
    std::fill_n(used.begin(), 25, uint8_t{1});
    const auto bytes = [](auto& mesh) { return reinterpret_cast<std::byte*>(mesh.data()); };
    const auto run = [&](float width, float height, std::span<const float> c, auto mesh,
                         std::span<const uint8_t> mask, uint32_t components = 3) {
        const auto extent = Measure(bytes(mesh), sizeof(Vertex), components, mask, c);
        const auto t = extent ? Fit(width, height, c, *extent) : std::nullopt;
        if (t) Apply(bytes(mesh), sizeof(Vertex), components, mask, *t);
        return std::pair{t, mesh};
    };
    const auto pixel_camera = [](float width, float height) {
        std::array<float, 56> c{};
        c[0] = std::bit_cast<float>(256u);
        c[4] = 1.92108f * height / width; c[9] = 1.92108f;
        c[14] = c[15] = -1.00002f; c[18] = -1;
        c[20] = c[25] = c[30] = c[32] = c[37] = c[42] = 1;
        c[31] = c[43] = c[9] * height / 2;
        c[44] = c[54] = 1; c[49] = 1.2f; // the paper fog's own overscan
        c[55] = -c[31];
        return c;
    };
    for (const auto& [width, height] : kNative) {
        const float s = height / 1080;
        const auto constants = pixel_camera(width, height);
        assert(Camera(width, height, constants));
        const auto [t, output] = run(width, height, constants, fog, used);
        assert(t);
        for (unsigned i = 0; i < 25; ++i) {
            // Full-width sheet: stock normalized coverage on the native canvas, including
            // the vertical overscan from the existing 1.2 Y transform.
            assert(std::abs(output[i][0] / width - fog[i][0] / 1920) < .000001f);
            assert(std::abs(output[i][1] * 1.2f / height - fog[i][1] * 1.2f / 1080) < .000001f);
            assert(!std::memcmp(output[i].data() + 2, fog[i].data() + 2, 14 * sizeof(float)));
        }
        assert(!std::memcmp(output.data() + 25, fog.data() + 25, 7 * sizeof(Vertex)));
        // Any sheet scale or resource that still covers the stock width is the same sheet.
        auto other = constants; other[44] = 1.5f; other[49] = 2;
        assert(run(width, height, other, fog, used).first->kx == width / 1920);

        // Other pixel-camera content keeps its exact stock framing (centred 16:9 area):
        // a centred panel, an off-centre one and a non-flat full-width mesh.
        std::array<Vertex, 4> panel{};
        for (unsigned i = 0; i < 4; ++i) {
            panel[i][0] = i & 1 ? 400 : -400; panel[i][1] = i & 2 ? 300 : -300;
            panel[i][2] = i == 3 ? 20 : 0; panel[i][9] = .5f;
        }
        std::array<uint8_t, 4> all{1, 1, 1, 1};
        auto offset = constants; offset[47] = 600; offset[51] = -200;
        for (const auto& c : {constants, offset}) {
            const auto [panel_t, moved] = run(width, height, c, panel, all);
            assert(s == 1 ? !panel_t : bool(panel_t));
            for (unsigned i = 0; i < 4; ++i) {
                const float x = c[44] * panel[i][0] + c[47], y = c[49] * panel[i][1] + c[51];
                assert(std::abs(c[44] * moved[i][0] + c[47] - x * s) < .001f);
                assert(std::abs(c[49] * moved[i][1] + c[51] - y * s) < .001f);
                assert(std::abs(moved[i][2] - panel[i][2] * s) < .0001f);
                assert(moved[i][9] == panel[i][9]);
            }
        }
        auto raised = fog; raised[12][2] = 40;
        const auto [raised_t, raised_out] = run(width, height, constants, raised, used);
        assert(s == 1 ? !raised_t : raised_t->kx == s && raised_t->kz == s);
        // Two-component positions: a flat sheet.
        assert(run(width, height, constants, fog, used, 2).first->kx == width / 1920);

        // Exclusions: the game camera, rotated views or worlds, a world origin off the
        // camera plane, a mismatched projection, invalid constants and stock 1920x1080.
        auto bad = constants; bad[9] = 2.748f; bad[4] = bad[9] * height / width;
        assert(!Camera(width, height, bad));
        bad = constants; bad[21] = .1f; assert(!Camera(width, height, bad));
        bad = constants; bad[38] = .1f; assert(!Camera(width, height, bad));
        bad = constants; bad[23] = 5; assert(!Camera(width, height, bad));
        bad = constants; bad[45] = .1f; assert(!Camera(width, height, bad));
        bad = constants; bad[49] = -1.2f; assert(!Camera(width, height, bad));
        bad = constants; bad[55] = 0; assert(!Camera(width, height, bad));
        bad = constants; bad[43] = 22; assert(!Camera(width, height, bad));
        bad = constants; bad[4] *= 2; assert(!Camera(width, height, bad));
        bad = constants; bad[50] = std::numeric_limits<float>::quiet_NaN();
        assert(!Camera(width, height, bad));
        assert(!Camera(width, height, std::span(constants).first(55)));
        assert(!Camera(1920, 1080, constants) && !Camera(1920, 1080, pixel_camera(1920, 1080)));
        if (height != 1440) assert(!Camera(2560, 1440, constants));
        auto broken = fog; broken[3][1] = std::numeric_limits<float>::infinity();
        assert(!Measure(bytes(broken), sizeof(Vertex), 3, used, constants));
        std::array<uint8_t, 32> none{};
        assert(!Measure(bytes(fog), sizeof(Vertex), 3, none, constants));
        assert(!Measure(bytes(fog), 8, 3, used, constants));
    }
    std::puts("PASS: pixel-camera content on every native canvas: full-width paper-fog sheets of any "
              "scale span the canvas, other content keeps stock framing (centred, off-centre, 3D); "
              "unused vertices and other attributes untouched; game-camera/rotated/off-plane/"
              "invalid/default16:9 exclusions");
}
