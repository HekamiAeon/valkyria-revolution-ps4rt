// SPDX-License-Identifier: GPL-2.0-or-later
// Copy extents between images that share guest memory (texture_cache/aliasing.h); adapted from
// cuesta4/shadPS4 (eltutz) tests/test_aliasing.cpp. No game, Vulkan device or window required.
#include <cassert>
#include <cstdio>

#include "video_core/amdgpu/resource.h"
#include "video_core/texture_cache/aliasing.h"
#include "video_core/texture_cache/image_info.h"

using namespace VideoCore;

static ImageInfo Make(Extent3D size, vk::Format format = vk::Format::eR8G8B8A8Unorm) {
    ImageInfo info{};
    info.guest_address = 0x100000;
    info.num_bits = 32;
    info.pixel_format = format;
    info.type = AmdGpu::ImageType::Color2D;
    info.pitch = size.width;
    info.size = size;
    return info;
}

int main() {
    // Same format: the smaller image is fully covered by the larger one.
    {
        const ImageInfo src = Make({1920, 1088, 1}), dst = Make({1920, 1080, 1});
        assert(GetAliasCopyExtent(src, dst) == dst.size);
        assert(GetAliasCopyExtent(dst, src) == dst.size);
    }
    // Compatible views of other formats: the common rows.
    {
        const ImageInfo src = Make({1920, 1088, 1}, vk::Format::eR8G8B8A8Uint);
        const ImageInfo dst = Make({1920, 1080, 1}, vk::Format::eR8G8B8A8Unorm);
        const Extent3D rows{1920, 1080, 1};
        assert(GetAliasCopyExtent(src, dst) == rows);
        assert(GetAliasCopyExtent(dst, src) == rows);
    }
    // Other memory layouts never alias.
    {
        const ImageInfo src = Make({1920, 1088, 1});
        ImageInfo dst = Make({1920, 1080, 1});
        dst.guest_address += 0x1000;
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1});
        ++dst.pitch;
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1});
        dst.props.is_depth = true;
        assert(!GetAliasCopyExtent(src, dst));
    }
    // Incompatible intersections.
    {
        const ImageInfo src = Make({1920, 1088, 1}, vk::Format::eR8G8B8A8Uint);
        ImageInfo dst = Make({1280, 1080, 1}, vk::Format::eR8G8B8A8Unorm);
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1}, vk::Format::eR16G16B16A16Sfloat);
        assert(!GetAliasCopyExtent(src, dst));
        dst = Make({1920, 1080, 1}, vk::Format::eR8G8B8A8Unorm);
        dst.resources.levels = 2;
        assert(!GetAliasCopyExtent(src, dst));
    }
    // Compressed textures from the game's pool: another size or tiling at a reused address is
    // a new texture (Valkyria street curbs sampled a view of a larger cached texture).
    {
        const auto bc = [](u32 width, u32 height, vk::Format format) {
            ImageInfo info = Make({width, height, 1}, format);
            info.num_bits = format == vk::Format::eBc1RgbaUnormBlock ? 64 : 128;
            info.props.is_block = true;
            info.props.is_tiled = true;
            info.tile_mode = AmdGpu::TileMode::Thin1DThin;
            info.array_mode = AmdGpu::ArrayMode::Array1DTiledThin1;
            return info;
        };
        const auto bc1 = vk::Format::eBc1RgbaUnormBlock, bc3 = vk::Format::eBc3UnormBlock;
        const ImageInfo cached = bc(1024, 1024, bc1);
        assert(IsCompressedPoolReuse(bc(512, 512, bc1), cached));
        assert(IsCompressedPoolReuse(cached, bc(512, 512, bc1)));
        assert(IsCompressedPoolReuse(bc(1024, 512, bc1), cached));
        assert(IsCompressedPoolReuse(bc(256, 1024, bc1), cached));
        ImageInfo retiled = bc(1024, 1024, bc1);
        retiled.tile_mode = AmdGpu::TileMode::Display2DThin;
        assert(IsCompressedPoolReuse(retiled, cached));
        retiled = bc(1024, 1024, bc1);
        retiled.array_mode = AmdGpu::ArrayMode::ArrayLinearAligned;
        assert(IsCompressedPoolReuse(retiled, cached));
        retiled = bc(1024, 1024, bc1);
        retiled.props.is_tiled = false;
        assert(IsCompressedPoolReuse(retiled, cached));
        // The same texture with fewer mips or layers stays a view.
        ImageInfo fewer_mips = bc(1024, 1024, bc1);
        ImageInfo full_chain = bc(1024, 1024, bc1);
        full_chain.resources.levels = 11;
        assert(!IsCompressedPoolReuse(fewer_mips, full_chain));
        fewer_mips.resources.layers = 1;
        full_chain.resources.layers = 6;
        assert(!IsCompressedPoolReuse(fewer_mips, full_chain));
        // Other formats keep the existing reinterpretation rules, uncompressed images too.
        assert(!IsCompressedPoolReuse(bc(512, 512, bc3), cached));
        assert(!IsCompressedPoolReuse(Make({1920, 1080, 1}), Make({5120, 1440, 1})));
        ImageInfo uncompressed = Make({1920, 1080, 1});
        uncompressed.props.is_tiled = true;
        assert(!IsCompressedPoolReuse(uncompressed, Make({1920, 1088, 1})));
    }
    std::puts("aliasing-test: OK");
    return 0;
}
