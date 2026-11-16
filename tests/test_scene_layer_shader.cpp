// SPDX-License-Identifier: GPL-2.0-or-later
// Run the production scene-layer shader over the production jitter sequence. The synthetic layer
// is drawn the way the rasterizer draws one: a single sample per texel, moved by the frame's
// jitter, so its edges are stepped and the raw picture moves from frame to frame.
#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#include "video_core/host_shaders/scene_layer_comp.h"
#include "video_core/renderer_vulkan/motion_history.h"
#include "video_core/renderer_vulkan/scene_layer.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/texture_cache/image.h"
#include <vk_mem_alloc.h>

namespace {
// The layer fills the left of a wider scene, as a talk portrait does.
constexpr u32 W = 64, H = 48, SceneW = 128, OW = 120, OH = 90;
constexpr float Scale = float(OW) / W;

float Halton(u32 index, u32 base) {
    float f = 1.0f, result = 0.0f;
    for (u32 i = index; i > 0; i /= base) {
        f /= float(base);
        result += f * float(i % base);
    }
    return result;
}

float Half(u16 v) {
    const int exponent = (v >> 10) & 31;
    const float mantissa = float(v & 1023);
    if (exponent == 31) return mantissa ? std::numeric_limits<float>::quiet_NaN()
                                        : std::numeric_limits<float>::infinity();
    return (v & 0x8000 ? -1.f : 1.f) *
           (exponent == 0 ? std::ldexp(mantissa, -24)
                          : std::ldexp(1.f + mantissa / 1024.f, exponent - 15));
}

// A disc, in unjittered render pixels: edges at every angle.
bool Inside(float x, float y, float cx, float cy) {
    return (x - cx) * (x - cx) + (y - cy) * (y - cy) < 14.0f * 14.0f;
}

// An eye on the disc, from its centre: an iris with a hard rim, detail as fine as a filtered
// texture holds (four texels a period) and a highlight. It closes and opens with no motion of
// its own to follow.
constexpr float EyeX = -4.0f, EyeY = -2.0f;
u8 Red(float x, float y, float cx, float cy, bool eye) {
    if (!Inside(x, y, cx, cy)) return 0;
    const float dx = x - cx - EyeX, dy = y - cy - EyeY;
    if (eye && dx * dx + dy * dy < 0.8f * 0.8f) return 255;
    if (eye && dx * dx / 25.0f + dy * dy / 9.0f < 1.0f) {
        return u8(std::lround(89.0f + 64.0f * std::cos(1.5707963f * dx) *
                                          std::cos(1.5707963f * dy)));
    }
    return 204;
}

struct Picture {
    std::vector<float> alpha = std::vector<float>(OW * OH);
    std::vector<float> red = std::vector<float>(OW * OH);
    float mass = 0, x = 0, y = 0; // coverage and its centre, output pixels
    void Measure() {
        mass = x = y = 0;
        for (u32 p = 0; p < OW * OH; ++p) {
            mass += alpha[p];
            x += alpha[p] * (float(p % OW) + 0.5f);
            y += alpha[p] * (float(p / OW) + 0.5f);
        }
        x /= mass;
        y /= mass;
    }
};

// What the output should show: the disc's coverage of each output pixel.
Picture Reference(float cx, float cy, bool eye = false) {
    Picture out;
    for (u32 p = 0; p < OW * OH; ++p) {
        u32 inside = 0, red = 0;
        for (u32 s = 0; s < 256; ++s) {
            const float x = (float(p % OW) + (float(s % 16) + 0.5f) / 16.0f) / Scale;
            const float y = (float(p / OW) + (float(s / 16) + 0.5f) / 16.0f) / Scale;
            inside += Inside(x, y, cx, cy);
            red += Red(x, y, cx, cy, eye);
        }
        out.alpha[p] = float(inside) / 256.0f;
        out.red[p] = float(red) / (256.0f * 255.0f);
    }
    out.Measure();
    return out;
}

// How far the output pixels around the eye are from what they should show, on average.
float EyeError(const Picture& a, const Picture& b, float cx, float cy) {
    float sum = 0;
    u32 count = 0;
    for (u32 p = 0; p < OW * OH; ++p) {
        if (std::abs((float(p % OW) + 0.5f) / Scale - cx - EyeX) < 7.0f &&
            std::abs((float(p / OW) + 0.5f) / Scale - cy - EyeY) < 5.0f) {
            sum += std::abs(a.red[p] - b.red[p]);
            ++count;
        }
    }
    return sum / float(count);
}

float Error(const Picture& a, const Picture& b) {
    float sum = 0;
    for (u32 p = 0; p < OW * OH; ++p) sum += std::abs(a.alpha[p] - b.alpha[p]);
    return sum / b.mass; // per unit of the disc's area
}
} // namespace

int main() {
    using namespace Vulkan;
    Instance instance(0, false);
    static vk::detail::DynamicLoader loader;
    vk::detail::DispatchLoaderDynamic d;
    d.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    d.init(instance.GetInstance());
    d.init(instance.GetDevice());
    const auto device = instance.GetDevice();
    Scheduler scheduler(instance);
    // 0: layer, 1: depth, 2: motion, 3 and 4: the output-size copy, 5 and 6: the layer
    // blurred (a frame's is compared with the next), each read and written in turn.
    struct Target {
        vk::Format format;
        u32 width, height;
    };
    const std::array<Target, 7> targets{{{vk::Format::eR8G8B8A8Unorm, W, H},
                                         {vk::Format::eR32Sfloat, SceneW, H},
                                         {vk::Format::eR16G16Sfloat, SceneW, H},
                                         {vk::Format::eR16G16B16A16Sfloat, OW, OH},
                                         {vk::Format::eR16G16B16A16Sfloat, OW, OH},
                                         {vk::Format::eR16G16B16A16Sfloat, W, H},
                                         {vk::Format::eR16G16B16A16Sfloat, W, H}}};
    std::array<VideoCore::UniqueImage, 7> images;
    std::array<vk::UniqueImageView, 7> views;
    for (u32 i = 0; i < images.size(); ++i) {
        images[i] = VideoCore::UniqueImage(device, instance.GetAllocator());
        images[i].Create({.imageType = vk::ImageType::e2D, .format = targets[i].format,
            .extent = {targets[i].width, targets[i].height, 1}, .mipLevels = 1, .arrayLayers = 1,
            .samples = vk::SampleCountFlagBits::e1, .tiling = vk::ImageTiling::eOptimal,
            .usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled |
                     vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst});
        views[i] = device.createImageViewUnique({.image = vk::Image(images[i]),
            .viewType = vk::ImageViewType::e2D, .format = targets[i].format,
            .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}, nullptr, d).value;
    }
    std::array<vk::DescriptorSetLayoutBinding, 7> bindings{};
    for (u32 i = 0; i < bindings.size(); ++i) {
        bindings[i] = {.binding = i,
            .descriptorType = i < 5 ? vk::DescriptorType::eCombinedImageSampler
                                    : vk::DescriptorType::eStorageImage,
            .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eCompute};
    }
    auto descriptors = device.createDescriptorSetLayoutUnique({
        .flags = vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR,
        .bindingCount = u32(bindings.size()), .pBindings = bindings.data()}, nullptr, d).value;
    const vk::PushConstantRange push{vk::ShaderStageFlagBits::eCompute, 0, 24};
    auto layout = device.createPipelineLayoutUnique({.setLayoutCount = 1,
        .pSetLayouts = &*descriptors, .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push}, nullptr, d).value;
    auto module = device.createShaderModuleUnique({.codeSize = sizeof(SCENE_LAYER_COMP),
        .pCode = SCENE_LAYER_COMP}, nullptr, d).value;
    auto pipeline = device.createComputePipelineUnique({}, {
        .stage = {.stage = vk::ShaderStageFlagBits::eCompute, .module = *module, .pName = "main"},
        .layout = *layout}, nullptr, d).value;
    auto sampler = device.createSamplerUnique({.magFilter = vk::Filter::eNearest,
        .minFilter = vk::Filter::eNearest, .mipmapMode = vk::SamplerMipmapMode::eNearest,
        .addressModeU = vk::SamplerAddressMode::eClampToEdge,
        .addressModeV = vk::SamplerAddressMode::eClampToEdge,
        .addressModeW = vk::SamplerAddressMode::eClampToEdge}, nullptr, d).value;
    constexpr size_t LayerBytes = W * H * 4, SceneBytes = SceneW * H * 4, CopyBytes = OW * OH * 8;
    constexpr size_t DepthAt = LayerBytes, MotionAt = DepthAt + SceneBytes,
                     CopyAt = MotionAt + SceneBytes;
    VkBuffer staging{};
    VmaAllocation allocation{};
    VmaAllocationInfo ai{};
    const VkBufferCreateInfo bi{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = CopyAt + CopyBytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT};
    const VmaAllocationCreateInfo ac{.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
        VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT, .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
    assert(vmaCreateBuffer(instance.GetAllocator(), &bi, &ac, &staging, &allocation, &ai) ==
           VK_SUCCESS);
    auto* const bytes = static_cast<u8*>(ai.pMappedData);
    const auto barrier = [&](vk::CommandBuffer cmd, vk::PipelineStageFlags2 source,
                             vk::AccessFlags2 source_access, vk::PipelineStageFlags2 dest,
                             vk::AccessFlags2 dest_access) {
        const vk::MemoryBarrier2 b{.srcStageMask = source, .srcAccessMask = source_access,
            .dstStageMask = dest, .dstAccessMask = dest_access};
        cmd.pipelineBarrier2({.memoryBarrierCount = 1, .pMemoryBarriers = &b}, d);
    };
    bool laid_out = false;
    u32 next = 0; // the copy written by the next frame
    std::array<float, 2> last_jitter{};
    // One frame: the disc centred at (cx, cy), drawn with `jitter` in front of nothing; its
    // texels carry `motion` (pixels to where they were). `poison` fills the copy read as
    // history with NaN first.
    const auto frame = [&](float cx, float cy, std::array<float, 2> jitter,
                           std::array<float, 2> motion, u32 age, bool reset,
                           bool poison = false, bool eye = false) {
        auto* depth_data = reinterpret_cast<float*>(bytes + DepthAt);
        auto* motion_data = reinterpret_cast<u16*>(bytes + MotionAt);
        std::fill_n(depth_data, SceneW * H, 1.0f);
        std::memset(motion_data, 0, SceneBytes);
        const auto half = [](float v) -> u16 { // exact for the test's multiples of 1/1024
            if (v == 0) return 0;
            int exponent;
            const float m = std::frexp(std::abs(v), &exponent); // [0.5, 1)
            return u16((v < 0 ? 0x8000 : 0) | ((exponent + 14) << 10) |
                       (u32(std::lround((m * 2 - 1) * 1024)) & 1023));
        };
        for (u32 p = 0; p < W * H; ++p) {
            // The rasterizer's sample of this texel: the scene `jitter` away from its centre.
            const bool in = Inside(float(p % W) + 0.5f - jitter[0],
                                   float(p / W) + 0.5f - jitter[1], cx, cy);
            const std::array<u8, 4> texel =
                in ? std::array<u8, 4>{Red(float(p % W) + 0.5f - jitter[0],
                                           float(p / W) + 0.5f - jitter[1], cx, cy, eye),
                                       128, 51, 255}
                   : std::array<u8, 4>{};
            std::memcpy(bytes + p * 4, texel.data(), 4);
            if (in) {
                depth_data[p / W * SceneW + p % W] = 0.5f;
                motion_data[(p / W * SceneW + p % W) * 2] = half(motion[0]);
                motion_data[(p / W * SceneW + p % W) * 2 + 1] = half(motion[1]);
            }
        }
        if (poison) std::fill_n(reinterpret_cast<u16*>(bytes + CopyAt), OW * OH * 4, u16(0x7e00));
        const auto cmd = scheduler.CommandBuffer();
        if (!laid_out) {
            std::array<vk::ImageMemoryBarrier2, 7> transitions{};
            for (u32 i = 0; i < 7; ++i) {
                transitions[i] = {.srcStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                    .dstStageMask = vk::PipelineStageFlagBits2::eAllCommands,
                    .dstAccessMask = vk::AccessFlagBits2::eMemoryRead |
                                     vk::AccessFlagBits2::eMemoryWrite,
                    .oldLayout = vk::ImageLayout::eUndefined,
                    .newLayout = vk::ImageLayout::eGeneral, .image = vk::Image(images[i]),
                    .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
            }
            cmd.pipelineBarrier2({.imageMemoryBarrierCount = 7,
                                  .pImageMemoryBarriers = transitions.data()}, d);
            laid_out = true;
        }
        barrier(cmd, vk::PipelineStageFlagBits2::eAllCommands,
                vk::AccessFlagBits2::eMemoryRead | vk::AccessFlagBits2::eMemoryWrite,
                vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite);
        const vk::ImageSubresourceLayers layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
        cmd.copyBufferToImage(staging, vk::Image(images[0]), vk::ImageLayout::eGeneral,
            vk::BufferImageCopy{.imageSubresource = layers, .imageExtent = {W, H, 1}}, d);
        cmd.copyBufferToImage(staging, vk::Image(images[1]), vk::ImageLayout::eGeneral,
            vk::BufferImageCopy{.bufferOffset = DepthAt, .imageSubresource = layers,
                                .imageExtent = {SceneW, H, 1}}, d);
        cmd.copyBufferToImage(staging, vk::Image(images[2]), vk::ImageLayout::eGeneral,
            vk::BufferImageCopy{.bufferOffset = MotionAt, .imageSubresource = layers,
                                .imageExtent = {SceneW, H, 1}}, d);
        if (poison) {
            cmd.copyBufferToImage(staging, vk::Image(images[4 - next]), vk::ImageLayout::eGeneral,
                vk::BufferImageCopy{.bufferOffset = CopyAt, .imageSubresource = layers,
                                    .imageExtent = {OW, OH, 1}}, d);
        }
        barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite);
        const std::array<vk::ImageView, 7> bound{*views[0], *views[1], *views[2],
                                                 *views[4 - next], *views[6 - next],
                                                 *views[3 + next], *views[5 + next]};
        std::array<vk::DescriptorImageInfo, 7> infos{};
        std::array<vk::WriteDescriptorSet, 7> writes{};
        for (u32 i = 0; i < 7; ++i) {
            infos[i] = {.sampler = i < 5 ? *sampler : vk::Sampler{}, .imageView = bound[i],
                        .imageLayout = vk::ImageLayout::eGeneral};
            writes[i] = {.dstBinding = i, .descriptorCount = 1,
                         .descriptorType = bindings[i].descriptorType, .pImageInfo = &infos[i]};
        }
        struct Params {
            std::array<float, 2> jitter, previous_jitter;
            float newest;
            u32 reset;
        } params{jitter, last_jitter, SceneLayer::NewestWeight(age), reset ? 1u : 0u};
        last_jitter = jitter;
        cmd.bindPipeline(vk::PipelineBindPoint::eCompute, *pipeline, d);
        cmd.pushDescriptorSetKHR(vk::PipelineBindPoint::eCompute, *layout, 0, writes, d);
        cmd.pushConstants(*layout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(params), &params,
                          d);
        cmd.dispatch((OW + 7) / 8, (OH + 7) / 8, 1, d);
        barrier(cmd, vk::PipelineStageFlagBits2::eComputeShader,
                vk::AccessFlagBits2::eShaderWrite, vk::PipelineStageFlagBits2::eTransfer,
                vk::AccessFlagBits2::eTransferRead);
        cmd.copyImageToBuffer(vk::Image(images[3 + next]), vk::ImageLayout::eGeneral, staging,
            vk::BufferImageCopy{.bufferOffset = CopyAt, .imageSubresource = layers,
                                .imageExtent = {OW, OH, 1}}, d);
        barrier(cmd, vk::PipelineStageFlagBits2::eTransfer, vk::AccessFlagBits2::eTransferWrite,
                vk::PipelineStageFlagBits2::eHost, vk::AccessFlagBits2::eHostRead);
        scheduler.Finish();
        next = 1 - next;
        Picture out;
        const auto* texels = reinterpret_cast<const u16*>(bytes + CopyAt);
        for (u32 p = 0; p < OW * OH; ++p) {
            for (u32 c = 0; c < 4; ++c) assert(std::isfinite(Half(texels[p * 4 + c])));
            out.alpha[p] = Half(texels[p * 4 + 3]);
            out.red[p] = Half(texels[p * 4]);
            // Colour follows coverage: the layer's channels are gathered alike.
            assert(eye || std::abs(out.red[p] - out.alpha[p] * 0.8f) < 0.004f);
        }
        out.Measure();
        return out;
    };
    const u32 phases = Motion::JitterPhases(W, OW);
    assert(phases == 29);
    u32 jitter_index = 0;
    const auto jitter = [&] {
        jitter_index = jitter_index % phases + 1;
        return std::array<float, 2>{Halton(jitter_index, 2) - 0.5f, Halton(jitter_index, 3) - 0.5f};
    };
    constexpr float CX = 30.3f, CY = 23.7f;
    const Picture reference = Reference(CX, CY);

    // The layer as it is shown without this shader: every frame alone, where the jitter put it.
    float raw_shake = 0, raw_error = 0;
    for (u32 f = 0; f < phases; ++f) {
        const auto j = jitter();
        Picture raw;
        for (u32 p = 0; p < OW * OH; ++p) {
            const u32 x = std::min(u32(float(p % OW) / Scale), W - 1);
            const u32 y = std::min(u32(float(p / OW) / Scale), H - 1);
            raw.alpha[p] = Inside(float(x) + 0.5f - j[0], float(y) + 0.5f - j[1], CX, CY);
        }
        raw.Measure();
        raw_shake = std::max(raw_shake, std::hypot(raw.x - reference.x, raw.y - reference.y));
        raw_error += Error(raw, reference) / float(phases);
    }
    std::printf("Scene layer: as drawn, the picture moves %.2f output pixels, error %.4f\n",
                raw_shake, raw_error);
    assert(raw_shake > 0.5f);

    // A first frame has only itself: put back where it belongs, whatever the copy held.
    jitter_index = 0;
    Picture out = frame(CX, CY, jitter(), {}, 0, true, true);
    const float first_error = Error(out, reference);
    std::printf("Scene layer: first frame %.3f pixels off, error %.4f\n",
                std::hypot(out.x - reference.x, out.y - reference.y), first_error);
    assert(std::hypot(out.x - reference.x, out.y - reference.y) < 0.2f);

    // A still layer: it must not move with the jitter, and its edges must fill in.
    std::vector<double> sum(OW * OH), squares(OW * OH);
    float shake = 0, error = 0;
    for (u32 f = 1; f < 4 * phases; ++f) {
        out = frame(CX, CY, jitter(), {}, f, false);
        if (f < 3 * phases) continue;
        shake = std::max(shake, std::hypot(out.x - reference.x, out.y - reference.y));
        error += Error(out, reference) / float(phases);
        for (u32 p = 0; p < OW * OH; ++p) {
            sum[p] += out.alpha[p];
            squares[p] += double(out.alpha[p]) * out.alpha[p];
        }
    }
    double flicker = 0;
    for (u32 p = 0; p < OW * OH; ++p) {
        const double mean = sum[p] / phases;
        flicker = std::max(flicker, std::sqrt(std::max(0.0, squares[p] / phases - mean * mean)));
    }
    std::printf("Scene layer: still, moves %.4f output pixels, error %.4f (first frame %.4f, "
                "as drawn %.4f), worst pixel flicker %.4f\n",
                shake, error, first_error, raw_error, flicker);
    assert(shake < 0.02f);
    assert(error < 0.5f * first_error && error < 0.25f * raw_error);
    assert(flicker < 0.06);

    // A layer in motion: followed, without its old edge trailing behind. Slow motion (an idle
    // pose breathing) keeps most of what the still layer gained; fast motion keeps what one
    // frame holds.
    float cx = CX, cy = CY;
    const auto moving = [&](std::array<float, 2> speed, float& off, float& wrong) {
        off = wrong = 0;
        for (u32 f = 0; f < 3 * phases; ++f) {
            cx += speed[0];
            cy += speed[1];
            out = frame(cx, cy, jitter(), {-speed[0], -speed[1]}, 200, false);
            if (f < 2 * phases) continue;
            const Picture now = Reference(cx, cy);
            off = std::max(off, std::hypot(out.x - now.x, out.y - now.y));
            wrong += Error(out, now) / float(phases);
        }
    };
    float slow_off, slow_error, fast_off, fast_error;
    moving({0.03125f, 0.015625f}, slow_off, slow_error);
    moving({0.25f, 0.125f}, fast_off, fast_error);
    std::printf("Scene layer: moving slowly, %.4f output pixels off, error %.4f; fast, %.4f "
                "off, error %.4f\n", slow_off, slow_error, fast_off, fast_error);
    // Off by what one frame's stepped edges are (the first frame above), not by the jitter.
    assert(slow_off < 0.25f && slow_error < 0.7f * first_error);
    assert(fast_off < 0.25f && fast_error < first_error);

    // Another picture in the copy without its motion (a pose that jumped): the copy starts
    // again from the new frame, as good as a first frame, and fills in from there.
    const Picture moved = Reference(CX + 20, CY);
    for (u32 f = 0; f <= phases; ++f) {
        out = frame(CX + 20, CY, jitter(), {}, 200, false);
        const float off = std::hypot(out.x - moved.x, out.y - moved.y), wrong = Error(out, moved);
        if (f != 0 && f != 8 && f != phases) continue;
        std::printf("Scene layer: replaced picture, frame %u: %.3f pixels off, error %.4f\n",
                    f + 1, off, wrong);
        assert(f != 0 || (off < 0.2f && wrong < 1.1f * first_error));
        assert(f != 8 || (off < 0.2f && wrong < 0.9f * first_error));
        assert(f != phases || (off < 0.2f && wrong < 0.6f * first_error));
    }

    // Unusable history is not blended.
    out = frame(CX, CY, jitter(), {}, 200, false, true);
    assert(std::hypot(out.x - reference.x, out.y - reference.y) < 0.2f);

    // A blink: detail that changes where it is, with no motion to follow and nothing in the
    // new frame's texels to bound the old ones by (an iris holds dark and bright alike). The
    // copy must not keep showing what was there.
    const Picture open = Reference(CX, CY, true);
    out = frame(CX, CY, jitter(), {}, 0, true, false, true);
    const float eye_first = EyeError(out, open, CX, CY);
    std::fill(sum.begin(), sum.end(), 0.0);
    std::fill(squares.begin(), squares.end(), 0.0);
    float eye_still = 0;
    for (u32 f = 1; f < 4 * phases; ++f) {
        out = frame(CX, CY, jitter(), {}, f, false, false, true);
        if (f < 3 * phases) continue;
        eye_still += EyeError(out, open, CX, CY) / float(phases);
        for (u32 p = 0; p < OW * OH; ++p) {
            sum[p] += out.red[p];
            squares[p] += double(out.red[p]) * out.red[p];
        }
    }
    double eye_flicker = 0;
    for (u32 p = 0; p < OW * OH; ++p) {
        const double mean = sum[p] / phases;
        eye_flicker = std::max(eye_flicker,
                               std::sqrt(std::max(0.0, squares[p] / phases - mean * mean)));
    }
    std::printf("Scene layer: eye, first frame error %.4f, still %.4f, worst pixel flicker "
                "%.4f\n", eye_first, eye_still, eye_flicker);
    // Still, its detail fills in like the outline's: the jitter alone is not a change.
    assert(eye_still < 0.5f * eye_first && eye_flicker < 0.05);
    float closed_error = 0;
    for (u32 f = 0; f < 6; ++f) {
        out = frame(CX, CY, jitter(), {}, 200, false);
        closed_error = std::max(closed_error, EyeError(out, reference, CX, CY));
    }
    std::array<float, 3> opened{};
    for (u32 f = 0; f <= phases; ++f) {
        out = frame(CX, CY, jitter(), {}, 200, false, false, true);
        if (f == 0 || f == 4 || f == phases) {
            opened[f == 0 ? 0 : f == 4 ? 1 : 2] = EyeError(out, open, CX, CY);
        }
    }
    std::printf("Scene layer: blink, closed error %.4f; opened again, frame 1 error %.4f, "
                "frame 5 %.4f, frame %u %.4f\n", closed_error, opened[0], opened[1],
                phases + 1, opened[2]);
    // Closed, nothing of the iris is left; opened, the eye is what one frame shows of it (no
    // closed lid over it) and fills in again.
    assert(closed_error < 0.01f);
    assert(opened[0] < 1.1f * eye_first && opened[1] < eye_first &&
           opened[2] < 0.6f * eye_first);

    assert(SceneLayer::NewestWeight(0) == 1.0f && SceneLayer::NewestWeight(1000) == 0.1f);
    assert(SceneLayer::OutputExtent(512, 1024, 1920) == 960 &&
           SceneLayer::OutputExtent(576, 576, 1080) == 1080 &&
           SceneLayer::OutputExtent(1920, 3840, 5120) == 2560 &&
           SceneLayer::OutputExtent(512, 0, 1920) == 0);
    // Jitter only when the frame's one layer was gathered: otherwise it is shown as drawn.
    assert(SceneLayer::KeepsJitter(1, true) && !SceneLayer::KeepsJitter(1, false) &&
           !SceneLayer::KeepsJitter(2, true) && !SceneLayer::KeepsJitter(0, true));
    vmaDestroyBuffer(instance.GetAllocator(), staging, allocation);
    std::printf("Scene layer shader: PASS\n");
}
