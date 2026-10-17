// SPDX-License-Identifier: GPL-2.0-or-later
// Exercise the production AA -> FSR -> presenter-color pass on synthetic images.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_presenter.h"
#include "video_core/renderer_vulkan/host_passes/fsr_pass.h"
#include "video_core/renderer_vulkan/host_passes/pp_pass.h"
#include <vk_mem_alloc.h>


int main() {
    using namespace Vulkan;
    Instance instance(0,false);
    const auto device=instance.GetDevice();
    static vk::detail::DynamicLoader loader;
    vk::detail::DispatchLoaderDynamic dispatch;
    dispatch.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    dispatch.init(instance.GetInstance()); dispatch.init(device);
    Scheduler scheduler(instance);
    HostPasses::FsrPass spatial;
    spatial.Create(device,instance.GetAllocator(),1);
    HostPasses::PostProcessingPass pp;
    pp.Create(device,vk::Format::eR32G32B32A32Sfloat);
    const auto run=[&](const char* label,u32 w,u32 h,u32 ow,u32 oh,bool aa,bool fsr,bool edge,bool encoded=false) {
        VideoCore::UniqueImage input(device,instance.GetAllocator()),output(device,instance.GetAllocator());
        const auto create=[&](VideoCore::UniqueImage& image,u32 width,u32 height) {
            image.Create({.imageType=vk::ImageType::e2D,.format=vk::Format::eR32G32B32A32Sfloat,
                .extent={width,height,1},.mipLevels=1,.arrayLayers=1,.samples=vk::SampleCountFlagBits::e1,
                .tiling=vk::ImageTiling::eOptimal,
                .usage=vk::ImageUsageFlagBits::eSampled|vk::ImageUsageFlagBits::eColorAttachment|
                       vk::ImageUsageFlagBits::eTransferSrc|vk::ImageUsageFlagBits::eTransferDst});
            return device.createImageViewUnique({.image=image,.viewType=vk::ImageViewType::e2D,
                .format=vk::Format::eR32G32B32A32Sfloat,
                .subresourceRange={vk::ImageAspectFlagBits::eColor,0,1,0,1}},nullptr,dispatch).value;
        };
        auto input_view=create(input,w,h),output_view=create(output,ow,oh);
        const size_t input_bytes=size_t(w)*h*16,output_bytes=size_t(ow)*oh*16;
        VkBuffer staging{}; VmaAllocation allocation{}; VmaAllocationInfo mapped{};
        const VkBufferCreateInfo buffer{.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size=input_bytes+output_bytes,.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT};
        const VmaAllocationCreateInfo memory{.flags=VMA_ALLOCATION_CREATE_MAPPED_BIT|
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,.usage=VMA_MEMORY_USAGE_AUTO,
            .requiredFlags=VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        assert(vmaCreateBuffer(instance.GetAllocator(),&buffer,&memory,&staging,&allocation,&mapped)==VK_SUCCESS);
        auto* source=static_cast<float*>(mapped.pMappedData);
        for (u32 y=0;y<h;++y) for (u32 x=0;x<w;++x) {
            const size_t p=(size_t(y)*w+x)*4;
            source[p]=source[p+1]=source[p+2]=edge ? (x>y*2 ? 1.f : 0.f) : 0.25f;
            source[p+3]=0.625f;
        }
        const auto cmd=scheduler.CommandBuffer();
        const auto transition=[&](vk::Image image,vk::ImageLayout before,vk::ImageLayout after,
                                  vk::PipelineStageFlags2 stage,vk::AccessFlags2 access) {
            const vk::ImageMemoryBarrier2 barrier{
                .srcStageMask=vk::PipelineStageFlagBits2::eAllCommands,
                .srcAccessMask=vk::AccessFlagBits2::eMemoryRead|vk::AccessFlagBits2::eMemoryWrite,
                .dstStageMask=stage,.dstAccessMask=access,.oldLayout=before,.newLayout=after,
                .image=image,.subresourceRange={vk::ImageAspectFlagBits::eColor,0,1,0,1}};
            cmd.pipelineBarrier2({.imageMemoryBarrierCount=1,.pImageMemoryBarriers=&barrier},dispatch);
        };
        transition(input,vk::ImageLayout::eUndefined,vk::ImageLayout::eTransferDstOptimal,
                   vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferWrite);
        const vk::BufferImageCopy upload{.imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},
            .imageExtent={w,h,1}};
        cmd.copyBufferToImage(staging,input,vk::ImageLayout::eTransferDstOptimal,upload,dispatch);
        transition(input,vk::ImageLayout::eTransferDstOptimal,vk::ImageLayout::eShaderReadOnlyOptimal,
                   vk::PipelineStageFlagBits2::eComputeShader|vk::PipelineStageFlagBits2::eFragmentShader,
                   vk::AccessFlagBits2::eShaderRead);
        auto view=spatial.Render(cmd,*input_view,{w,h},{ow,oh},
            {.enable=fsr,.use_rcas=fsr,.rcas_attenuation=1.3f,.anti_aliasing=aa,.srgb_input=encoded},false);
        transition(output,vk::ImageLayout::eUndefined,vk::ImageLayout::eColorAttachmentOptimal,
                   vk::PipelineStageFlagBits2::eColorAttachmentOutput,vk::AccessFlagBits2::eColorAttachmentWrite);
        Frame frame{}; frame.width=ow; frame.height=oh; frame.image=output; frame.image_view=*output_view;
        // Bypass output encoding so this test can inspect the filter's actual linear values.
        pp.Render(cmd,view,{ow,oh},frame,{.gamma=1,.hdr=1,.srgb_input=0});
        transition(output,vk::ImageLayout::eGeneral,vk::ImageLayout::eTransferSrcOptimal,
                   vk::PipelineStageFlagBits2::eTransfer,vk::AccessFlagBits2::eTransferRead);
        const vk::BufferImageCopy download{.bufferOffset=input_bytes,
            .imageSubresource={vk::ImageAspectFlagBits::eColor,0,0,1},.imageExtent={ow,oh,1}};
        cmd.copyImageToBuffer(output,vk::ImageLayout::eTransferSrcOptimal,staging,download,dispatch);
        const vk::MemoryBarrier2 host{.srcStageMask=vk::PipelineStageFlagBits2::eTransfer,
            .srcAccessMask=vk::AccessFlagBits2::eTransferWrite,.dstStageMask=vk::PipelineStageFlagBits2::eHost,
            .dstAccessMask=vk::AccessFlagBits2::eHostRead};
        cmd.pipelineBarrier2({.memoryBarrierCount=1,.pMemoryBarriers=&host},dispatch);
        scheduler.Finish();
        const auto* pixels=source+input_bytes/4;
        size_t intermediate=0;
        for (size_t p=0;p<size_t(ow)*oh;++p) {
            for (u32 c=0;c<4;++c) assert(std::isfinite(pixels[p*4+c]));
            const float value=pixels[p*4];
            assert(value>=-0.01f && value<=1.01f);
            if (!edge) assert(std::abs(value-0.25f)<0.004f);
            if (edge && value>0.01f && value<0.99f) ++intermediate;
            if (!fsr) assert(std::abs(pixels[p*4+3]-0.625f)<0.001f);
        }
        if (edge) assert(aa ? intermediate>0 : intermediate==0);
        std::printf("PASS: %s (%ux%u -> %ux%u, %zu edge pixels smoothed)\n",label,w,h,ow,oh,intermediate);
        vmaDestroyBuffer(instance.GetAllocator(),staging,allocation);
    };
    run("AA off preserves staircase",33,17,33,17,false,false,true);
    run("AA smooths staircase, odd bounds",33,17,33,17,true,false,true);
    run("AA preserves flat linear gray",33,17,33,17,true,false,false);
    run("AA preserves flat encoded gray",33,17,33,17,true,false,false,true);
    run("720p AA/FSR/RCAS -> 4K",1280,720,3840,2160,true,true,false);
    // Reuse after size changes and disabling scaling, exercising image recreation.
    run("AA after 4K resize",35,19,35,19,true,false,true);
}
