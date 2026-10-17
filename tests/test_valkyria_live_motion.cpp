// SPDX-License-Identifier: GPL-2.0-or-later
// Starting with Original must still support object motion after enabling FSR live.
#include <cassert>
#include <cstdio>
#include "ps4rt_settings.h"
#include "video_core/renderer_vulkan/vk_instance.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_object_motion.h"

int main() {
    using namespace Ps4RtSettings;
    auto& settings = Get();
    settings.valkyria = true;
    SetValkyriaUpscaler(UpscalerOff);
    Vulkan::Instance instance(0, false);
    static vk::detail::DynamicLoader loader;
    vk::detail::DispatchLoaderDynamic dispatch;
    dispatch.init(loader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));
    dispatch.init(instance.GetInstance());
    Vulkan::Scheduler scheduler(instance);
    Vulkan::ObjectMotion motion(instance, scheduler);
    if (!instance.GetPhysicalDevice().getFeatures(dispatch).vertexPipelineStoresAndAtomics) {
        std::puts("Live motion: unsupported device; camera-only fallback");
        return 0;
    }
    assert(!motion.Enabled());
    const Vulkan::Motion::Draw draw{.shader = 1, .geometry = 2, .indices = 3, .topology = 4,
        .index_count = 3, .instances = 1, .vertices = {0, 3}};
    for (int provider : {UpscalerFsr3, UpscalerFsr4}) {
        SetValkyriaUpscaler(provider);
        assert(motion.Enabled());
        motion.OnFrameStart();
        assert(motion.PrepareDraw(draw) != 0);
        SetValkyriaUpscaler(UpscalerOff);
        assert(!motion.Enabled() && motion.PrepareDraw({}) == 0);
        motion.OnFrameStart();
    }
    std::puts("Live motion: Original startup, live FSR enable/disable and parameter writes PASS");
}
