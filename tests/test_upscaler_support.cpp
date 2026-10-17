// SPDX-License-Identifier: GPL-2.0-or-later
// Saved FSR 4 choices must be safe before the first rendered frame on older GPUs.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <unistd.h>
#include "ps4rt_settings.h"

int main() {
    using namespace Ps4RtSettings;
    char path[] = "/tmp/ps4rt-upscaler-test-XXXXXX";
    const int fd = mkstemp(path);
    assert(fd >= 0);
    close(fd);
    setenv("PS4RT_CONFIG", path, 1);
    unsetenv("PS4RT_UPSCALER");
    unsetenv("PS4RT_UPSCALE_PRESET");
    unsetenv("PS4RT_RENDER_RES");
    auto& s = Get();
    s.upscaler = UpscalerTaa;
    s.preset = Performance;
    assert(RenderPreset() == NativeAA && s.preset == Performance);
    Save();
    s.upscaler = UpscalerOff;
    Load();
    assert(s.upscaler == UpscalerTaa && s.preset == Performance);
    assert(RenderPreset() == NativeAA && !ResolutionNeedsRestart());
    s.upscaler = UpscalerFsr3;
    assert(RenderPreset() == Performance);
    for (bool fsr4 : {false, true}) {
        for (bool fsr411 : {false, true}) {
            for (int requested = 0; requested < UpscalerCount; ++requested) {
                FILE* config = std::fopen(path, "w");
                assert(config);
                std::fprintf(config, "upscaler=%s\npreset=3\noutput_res=2560x1440\n",
                             UpscalerName(requested));
                std::fclose(config);
                s.fsr4_problem = nullptr;
                Load();
                assert(s.upscaler == requested);
                ConfigureUpscalerSupport(fsr4, fsr411);
                const bool unsupported = (requested == UpscalerFsr4 && !fsr4) ||
                    (requested == UpscalerFsr411 && !(fsr4 && fsr411));
                assert(s.upscaler == (unsupported ? UpscalerFsr3 : requested));
                assert(s.fsr4_supported == fsr4);
                assert(s.fsr411_supported == (fsr4 && fsr411));
                assert(bool(s.fsr4_problem.load()) == unsupported);
                assert(s.preset == Performance && s.output_res == 2);
                // Startup patch settings still describe the already applied guest patches.
                assert(s.startup_upscaler == requested);
                ConfigureUpscalerSupport(fsr4, fsr411);
                assert(s.upscaler == (unsupported ? UpscalerFsr3 : requested));
            }
        }
    }
    s.startup_preset = Quality;
    s.startup_upscaler = UpscalerFsr3;
    s.upscaler = UpscalerFsr3;
    s.preset = Performance;
    assert(RenderPreset() == Performance && !ResolutionNeedsRestart());
    setenv("PS4RT_RENDER_RES", "1706x960", 1);
    assert(RenderPreset() == Quality && ResolutionNeedsRestart());
    s.preset = Quality;
    assert(!ResolutionNeedsRestart());
    s.upscaler = UpscalerOff;
    assert(ResolutionNeedsRestart());
    s.upscaler = UpscalerFsr4;
    assert(!ResolutionNeedsRestart()); // provider change at the same patched size is live
    s.upscaler = UpscalerTaa;          // TAA needs native guest targets
    assert(ResolutionNeedsRestart());
    s.upscaler = UpscalerFsr4;
    const int output = s.startup_output_res;
    s.output_res = (output + 1) % OutputCount;
    assert(ResolutionNeedsRestart());
    s.output_res = output;
    setenv("PS4RT_RENDER_RES", "", 1);
    assert(!FixedRenderSession() && !ResolutionNeedsRestart());
    unsetenv("PS4RT_RENDER_RES");
    // Valkyria selects providers explicitly and retains object motion on FSR 4 fallback.
    for (int provider : {UpscalerFsr3, UpscalerFsr4, UpscalerFsr411}) {
        s.spatial_aa = s.fsr1 = s.rcas = true;
        ConfigureValkyriaUpscaler(UpscalerName(provider), true);
        assert(s.upscaler == provider && s.startup_upscaler == provider);
        assert(s.jitter && s.object_motion && s.startup_object_motion);
        assert(!s.spatial_aa && !s.fsr1 && !s.rcas && !s.reactive);
        ConfigureUpscalerSupport(false, false);
        assert(s.upscaler == UpscalerFsr3 && s.object_motion && s.jitter);
    }
    for (const char* provider : {"off", "invalid"}) {
        s.spatial_aa = s.fsr1 = s.rcas = true;
        ConfigureValkyriaUpscaler(provider, true);
        assert(s.upscaler == UpscalerOff && s.startup_upscaler == UpscalerOff);
        assert(!s.jitter && !s.object_motion && !s.startup_object_motion);
        assert(!s.spatial_aa && !s.fsr1);
    }
    ConfigureValkyriaUpscaler("fsr3", false);
    assert(s.upscaler == UpscalerFsr3 && !s.object_motion && s.jitter);
    // User menu choices survive process-style reloads at every supported output.
    unsetenv("VALKYRIA_UPSCALER");
    unsetenv("VALKYRIA_OBJECT_MOTION");
    for (int provider : {UpscalerOff, UpscalerFsr3, UpscalerFsr4}) {
        for (int output : {1, 2, 3}) {
            s.valkyria = true;
            s.sharpen = true;
            s.sharpness = 0.42f;
            SetValkyriaUpscaler(provider);
            s.output_res = output;
            Save();
            s.upscaler = UpscalerTaa;
            Load(true);
            assert(s.upscaler == provider && s.startup_upscaler == provider);
            assert(s.output_res == output && s.sharpness > 0.419f && s.sharpness < 0.421f);
            assert(s.jitter == (provider != UpscalerOff));
            assert(s.object_motion == (provider != UpscalerOff));
            assert(!s.fsr1 && !s.spatial_aa);
            assert(s.rcas == (provider == UpscalerOff));
        }
    }
    // Explicit developer overrides still beat the saved menu choice.
    setenv("VALKYRIA_UPSCALER", "fsr3", 1);
    setenv("VALKYRIA_OBJECT_MOTION", "0", 1);
    Load(true);
    assert(s.upscaler == UpscalerFsr3 && !s.object_motion && s.jitter);
    unsetenv("VALKYRIA_UPSCALER");
    unsetenv("VALKYRIA_OBJECT_MOTION");
    { FILE* f = std::fopen(path, "w"); assert(f); std::fclose(f); }
    Load(true);
    assert(s.upscaler == UpscalerFsr3 && s.object_motion && s.output_res == 3);
    std::remove(path);
    std::puts("Upscaler support: saved choices and FSR 3.1 fallback PASS");
}
