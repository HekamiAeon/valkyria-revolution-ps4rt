// ps4rt.ini graphics keys (ps4rt_settings.cpp, ps4rt_graphics.h).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "ps4rt_graphics.h"
#include "ps4rt_settings.h"

namespace G = Ps4RtGraphics;
static void Env(const char* key,const char* value) {
#ifdef _WIN32
    _putenv_s(key,value);
#else
    if (*value) setenv(key,value,1); else unsetenv(key);
#endif
}

static std::string Read(const char* path) {
    std::ifstream in(path);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}

int main() {
    assert(G::ParseDisplayMode("borderless") == G::Borderless && G::ParseDisplayMode("bogus") == G::Windowed);
    assert(G::ParseFpsLimit(240) == 240 && G::ParseFpsLimit(75) == 0);
    assert(G::ParseResolution("3840x1440") == 7 && G::ParseResolution("5120x1440") == 9 &&
           G::ParseResolution("1440p") == 3 && G::ParseResolution("540p") == 2 &&
           G::ParseResolution("2160p") == 4 && G::ParseResolution("8K") == 0);
    assert(!std::strcmp(G::ResolutionLabel(2), "1024 x 576") && !std::strcmp(G::ResolutionLabel(0), "1920 x 1080") &&
           !std::strcmp(G::ResolutionLabel(4), "3840 x 2160 (4K)") && !std::strcmp(G::ResolutionLabel(99), "1920 x 1080"));
    assert(G::Resolutions.size() == G::ResolutionWidths.size() &&
           G::Resolutions.size() == G::ResolutionHeights.size() &&
           G::Resolutions.size() == G::ResolutionMenuLabels.size());
    {
        size_t listed = 0;
        for (const auto& group : G::ResolutionMenuGroups) listed += size_t(group.count);
        assert(listed == G::Resolutions.size() && G::ResolutionWidths[0] == 1920 && G::ResolutionHeights[0] == 1080);
    }
    assert(G::ParseEngineFps(240) == 240 && G::ParseEngineFps(90) == 120);
    assert(G::ParseAnisotropy(0) == 0 && G::ParseAnisotropy(2) == 2 &&
           G::ParseAnisotropy(4) == 4 && G::ParseAnisotropy(8) == 8 &&
           G::ParseAnisotropy(16) == 16 && G::ParseAnisotropy(3) == 0);
    assert(G::ParseFramesQueued(2) == 2 && G::ParseFramesQueued(0) == 0 && G::ParseFramesQueued(5) == 1);
    assert(G::ParseRcasStrength(100) == 100 && G::ParseRcasStrength(101) == 75 && G::ParseRcasStrength(-1) == 75);
    assert(G::RcasAttenuation(100) == 0.0f && G::RcasAttenuation(0) == 2.0f && G::RcasAttenuation(75) == 0.5f);
    assert(G::FsrPassRuns(true, false, 1280, 720, 2560, 1440) && !G::FsrPassRuns(false, false, 1280, 720, 2560, 1440));
    assert(G::FsrPassRuns(false, true, 1920, 1080, 1920, 1080) && !G::FsrPassRuns(true, false, 1920, 1080, 1920, 1080));
    assert(!G::FsrPassRuns(true, true, 2560, 1440, 1920, 1080));  // larger image: regular scaling
    assert(G::EffectiveFps(0, 120) == 120 && G::EffectiveFps(240, 120) == 120 && G::EffectiveFps(60, 240) == 60);
    // An unpatched engine steps 1/60 s per frame: presenting faster speeds the game up.
    assert(G::PresentLimit(120, 60) == 60 && G::PresentLimit(240, 120) == 120 && G::PresentLimit(240, 240) == 240);
    assert(G::PresentLimit(0, 60) == 0 && G::PresentLimit(30, 60) == 30);  // 0: paced by vblank

    const char* path = "graphics-settings-test.ini";
    {
        std::ofstream out(path);
        out << "display_mode=borderless\nvsync=0\nfps_limit=240\nasync_shaders=0\n"
               "render_resolution=3840x1440\nengine_fps=240\ndeferred_readback=0\nstale_readback=1\n"
               "fsr1=1\nrcas=0\nrcas_strength=40\nframes_queued=2\n";
    }
    Env("PS4RT_CONFIG", path);
    Env("PS4RT_ASYNC_SHADERS", "");
    auto& v = Ps4RtSettings::Get();
    Ps4RtSettings::Load();
    assert(v.display_mode == G::Borderless && !v.vsync && v.fps_limit == 240 && !v.async_shaders);
    assert(v.render_resolution == 7 && v.engine_fps == 240 && !v.deferred_readback && v.stale_readback);
    // FSR 1 left the menus in 2.2.0: a saved fsr1=1 (or PS4RT_FSR1) no longer turns it on.
    assert(!v.fsr1 && !v.rcas && v.rcas_strength == 40 && v.frames_queued == 2);
    Env("PS4RT_FSR1", "1");
    Ps4RtSettings::Load();
    assert(!v.fsr1);
    Env("PS4RT_FSR1", "");
    assert(!Ps4RtSettings::GraphicsNeedRestart());
    assert(v.applied_engine_fps == 240);  // no PS4RT_ENGINE_FPS: the configured rate
    Env("PS4RT_ENGINE_FPS", "60");    // run.py: the frame rate patch did not apply
    Ps4RtSettings::Load();
    assert(v.engine_fps == 240 && v.applied_engine_fps == 60 && !Ps4RtSettings::GraphicsNeedRestart());
    Env("PS4RT_ENGINE_FPS", "");
    v.engine_fps = 120;
    assert(Ps4RtSettings::GraphicsNeedRestart());
    v.fps_limit = 60;  // live: no restart because of it
    v.engine_fps = 240;
    assert(!Ps4RtSettings::GraphicsNeedRestart());

    Ps4RtSettings::Save();
    const std::string saved = Read(path);
    for (const char* line : {"display_mode=borderless\n", "vsync=0\n", "fps_limit=60\n", "async_shaders=0\n",
                             "render_resolution=3840x1440\n", "engine_fps=240\n", "deferred_readback=0\n",
                             "stale_readback=1\n", "fsr1=0\n", "rcas=0\n", "rcas_strength=40\n",
                             "frames_queued=2\n"}) {
        assert(saved.find(line) != std::string::npos);
    }

    {
        std::ofstream out(path);
        out << "display_mode=huge\nfps_limit=7\nrender_resolution=8K\nengine_fps=1\n"
               "rcas_strength=250\nframes_queued=9\n";
    }
    Env("PS4RT_ASYNC_SHADERS", "1");  // the environment still wins at start
    v.async_shaders = false;
    Ps4RtSettings::Load();
    assert(v.display_mode == G::Windowed && v.fps_limit == 0 && v.render_resolution == 0 && v.engine_fps == 120);
    assert(v.rcas_strength == 75 && v.frames_queued == 1);
    assert(v.async_shaders);
    {
        std::ofstream out(path);
        out << "legacy_unused_option=1\n";
    }
    Ps4RtSettings::Load();
    Ps4RtSettings::Save();
    const std::string rewritten = Read(path);
    assert(rewritten.find("legacy_unused_option") == std::string::npos);
    { std::ofstream empty(path); }
    Env("PS4RT_ASYNC_SHADERS", "");
    Ps4RtSettings::Load(true);
    assert(!v.async_shaders);
    {
        std::ofstream out(path);
        out << "async_shaders=1\n"; // Earlier Valkyria versions saved the old default.
    }
    Ps4RtSettings::Load(true);
    assert(!v.async_shaders);
    Ps4RtSettings::Load(false);
    assert(v.async_shaders); // Other profiles retain their saved preference.
    Env("PS4RT_ASYNC_SHADERS", "1");
    Ps4RtSettings::Load(true);
    assert(v.async_shaders); // Explicit opt-in remains available for diagnostics.
    Env("PS4RT_ASYNC_SHADERS", "0");
    Ps4RtSettings::Load(true);
    assert(!v.async_shaders);
    Env("PS4RT_ASYNC_SHADERS", "");
    { std::ofstream empty(path); }
    Ps4RtSettings::Load(true);
    assert(!v.async_shaders);
    assert(v.valkyria && !v.spatial_aa && !v.fsr1 && v.upscaler==Ps4RtSettings::UpscalerFsr3 && v.output_res==3);
    v.spatial_aa=false; v.fsr1=false; v.output_res=2;
    Ps4RtSettings::Save();
    Ps4RtSettings::Load(true);
    assert(!v.spatial_aa && !v.fsr1 && v.output_res==2);
    assert(v.anisotropy == 0 && v.startup_anisotropy == 0);
    v.anisotropy = 16;
    assert(Ps4RtSettings::GraphicsNeedRestart());
    Ps4RtSettings::Save();
    assert(Read(path).find("anisotropy=16\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.anisotropy == 16 && v.startup_anisotropy == 16);
    assert(!Ps4RtSettings::GraphicsNeedRestart());
    Env("VALKYRIA_AA","1");
    Ps4RtSettings::Load(true);
    assert(v.spatial_aa);
    Env("VALKYRIA_AA","");
    // Engine frame rate: the game's 30 unless 60 (experimental) is chosen, saved for Valkyria.
    // The frame rate patches load at every launch, so the choice applies at once, both ways,
    // and never asks for a restart; the next launch starts at the saved rate.
    assert(!v.sixty_fps && !v.startup_sixty_fps && !v.sixty_fps_problem && v.frame_rate_patches);
    assert(Ps4RtSettings::SixtyFpsLive());
    Ps4RtSettings::Save();
    assert(Read(path).find("experimental_60fps=0\nframe_rate_patches=1\n") != std::string::npos);
    v.sixty_fps = true; // 60 at once from a launch that started at 30
    assert(!Ps4RtSettings::GraphicsNeedRestart() && Ps4RtSettings::SixtyFpsLive());
    Ps4RtSettings::Save();
    assert(Read(path).find("experimental_60fps=1\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.sixty_fps && v.startup_sixty_fps && !Ps4RtSettings::GraphicsNeedRestart());
    assert(Ps4RtSettings::SixtyFpsLive());
    v.sixty_fps = false; // and 30 at once from a launch that started at 60
    assert(!Ps4RtSettings::GraphicsNeedRestart() && Ps4RtSettings::SixtyFpsLive());
    v.sixty_fps = true;
    assert(!Ps4RtSettings::GraphicsNeedRestart() && Ps4RtSettings::SixtyFpsLive());
    v.sixty_fps = false;
    Ps4RtSettings::Save();
    assert(Read(path).find("experimental_60fps=0\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(!v.sixty_fps && !v.startup_sixty_fps && Ps4RtSettings::SixtyFpsLive());
    Env("VALKYRIA_60FPS", "1");
    Ps4RtSettings::Load(true);
    assert(v.sixty_fps && v.startup_sixty_fps);
    Env("VALKYRIA_60FPS", "");
    Ps4RtSettings::Load(true);
    assert(!v.sixty_fps && !v.startup_sixty_fps);
    // A launch whose patches could not start is the unmodified game at 30 whatever the
    // choice: nothing is live and no restart is asked.
    Env("VALKYRIA_60FPS", "1");
    Ps4RtSettings::Load(true);
    assert(Ps4RtSettings::SixtyFpsLive());
    Ps4RtSettings::SixtyFpsUnavailable("game code differs at 0x3d319f");
    assert(v.sixty_fps_problem && !Ps4RtSettings::SixtyFpsLive() && !Ps4RtSettings::GraphicsNeedRestart());
    v.sixty_fps = false;
    assert(!Ps4RtSettings::GraphicsNeedRestart());
    Env("VALKYRIA_60FPS", "");
    v.sixty_fps_problem = nullptr;
    // frame_rate_patches=0 (settings file only) keeps the patches out: the unmodified game at
    // 30, nothing live, and the key stays as it was written. A file without the key has them.
    {
        std::ofstream out(path);
        out << "experimental_60fps=1\nframe_rate_patches=0\n";
    }
    Ps4RtSettings::Load(true);
    assert(v.sixty_fps && !v.frame_rate_patches && !Ps4RtSettings::SixtyFpsLive());
    assert(!Ps4RtSettings::GraphicsNeedRestart());
    Ps4RtSettings::Save();
    assert(Read(path).find("experimental_60fps=1\nframe_rate_patches=0\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(!v.frame_rate_patches);
    {
        std::ofstream out(path);
        out << "experimental_60fps=0\n";
    }
    Ps4RtSettings::Load(true);
    assert(!v.sixty_fps && v.frame_rate_patches && Ps4RtSettings::SixtyFpsLive());

    // A new render resolution is an attempt: marked in the file before the launch tries it, and
    // undone by the next launch when that one never reached the game.
    {
        std::ofstream out(path);
        out << "render_resolution=3440x1440\nupscaler=fsr3\n";
    }
    v.restart_unconfirmed = false;
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 6 && v.startup_render_resolution == 6 && v.restart_unconfirmed);
    assert(v.confirmed_render_resolution == 0);
    assert(Read(path).find("restart_unconfirmed=1\n") != std::string::npos &&
           Read(path).find("render_resolution=3440x1440\n") != std::string::npos &&
           Read(path).find("render_resolution_confirmed=native\n") != std::string::npos);
    Ps4RtSettings::Load(true); // the attempt never confirmed: back to native
    assert(v.render_resolution == 0 && v.startup_render_resolution == 0 && !v.restart_unconfirmed);
    assert(Read(path).find("render_resolution=native\n") != std::string::npos &&
           Read(path).find("restart_unconfirmed=0\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 0 && !v.restart_unconfirmed);
    // A launch that reaches the game confirms its resolution; a menu change made meanwhile is the
    // next attempt.
    v.render_resolution = 9;
    Ps4RtSettings::Save();
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 9 && v.restart_unconfirmed);
    v.render_resolution = 8;
    Ps4RtSettings::ConfirmRenderResolution();
    assert(v.confirmed_render_resolution == 9 && !v.restart_unconfirmed);
    assert(Read(path).find("render_resolution_confirmed=5120x1440\n") != std::string::npos &&
           Read(path).find("render_resolution=3840x1080\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 8 && v.restart_unconfirmed && v.confirmed_render_resolution == 9);
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 9 && !v.restart_unconfirmed);
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 9 && v.startup_render_resolution == 9 && !v.restart_unconfirmed);
    // 16:9 keys from the Sakura Wars list round-trip.
    v.render_resolution = 2;
    Ps4RtSettings::Save();
    assert(Read(path).find("render_resolution=540p\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 2 && v.restart_unconfirmed);
    Ps4RtSettings::ConfirmRenderResolution();
    Ps4RtSettings::Load(true);
    assert(v.render_resolution == 2 && v.confirmed_render_resolution == 2 && !v.restart_unconfirmed);

    // 1080p: Native is the scene; larger 16:9 outputs up to 3x; any other choice is Native.
    int w = 0, h = 0;
    const auto output = [&](int choice, int ew, int eh) {
        Ps4RtSettings::OutputSize(choice, w, h);
        assert(w == ew && h == eh);
    };
    Ps4RtSettings::SceneSize(w, h);
    assert(w == 1920 && h == 1080);
    output(0, 1920, 1080);
    output(1, 1920, 1080);
    output(2, 2560, 1440);
    output(3, 3840, 2160);
    for (int other : {4, 5, 6, 9, -1}) output(other, 1920, 1080);
    // The outputs each internal resolution offers (larger, same aspect within 1%, up to 3x).
    {
        namespace G = Ps4RtGraphics;
        const std::pair<int, std::vector<int>> offered[] = {
            {0, {0, 2, 3}}, {1, {0, 1, 2, 3}}, {2, {0, 1, 2}}, {3, {0, 3}}, {4, {0}},
            {5, {0, 4}}, {6, {0}}, {7, {0}}, {8, {0, 6}}, {9, {0}}};
        for (const auto& [resolution, outputs] : offered) {
            std::vector<int> allowed;
            for (int o = 0; o < Ps4RtSettings::OutputCount; ++o)
                if (Ps4RtSettings::OutputAllowed(G::ResolutionWidths[resolution], G::ResolutionHeights[resolution], o))
                    allowed.push_back(o);
            assert(allowed == outputs);
        }
        assert(!Ps4RtSettings::OutputAllowed(1920, 1080, -1) && !Ps4RtSettings::OutputAllowed(1920, 1080, 7) &&
               !Ps4RtSettings::OutputAllowed(0, 1080, 2));
    }
    // A canvas keeps the FSR choice and renders its scene at the canvas size; the output fits
    // the scene aspect (never stretched).
    assert(v.upscaler == Ps4RtSettings::UpscalerFsr3 && v.object_motion && v.jitter);
    Ps4RtSettings::ActivateCanvas(5120, 1440);
    assert(v.upscaler == Ps4RtSettings::UpscalerFsr3 && v.startup_upscaler == Ps4RtSettings::UpscalerFsr3 &&
           v.jitter && v.object_motion);
    Ps4RtSettings::SceneSize(w, h);
    assert(w == 5120 && h == 1440);
    for (int choice = 0; choice < Ps4RtSettings::OutputCount; ++choice) output(choice, 5120, 1440);
    Ps4RtSettings::ActivateCanvas(2560, 1080);
    output(4, 3414, 1440);
    output(3, 2560, 1080);
    output(6, 2560, 1080);
    Ps4RtSettings::ActivateCanvas(3840, 1080);
    output(6, 5120, 1440);
    output(5, 3840, 1080);
    Ps4RtSettings::ActivateCanvas(1280, 720);
    output(1, 1920, 1080);
    output(2, 2560, 1440);
    output(3, 3840, 2160);
    Ps4RtSettings::ActivateCanvas(1024, 576);
    output(1, 1920, 1080);
    output(2, 2560, 1440);
    output(3, 1024, 576); // 3.75x: beyond FSR's range
    Ps4RtSettings::ActivateCanvas(2560, 1440);
    output(2, 2560, 1440);
    output(3, 3840, 2160);
    Ps4RtSettings::ActivateCanvas(3840, 2160);
    for (int choice = 0; choice < Ps4RtSettings::OutputCount; ++choice) output(choice, 3840, 2160);
    Ps4RtSettings::Save();
    assert(Read(path).find("upscaler=fsr3\n") != std::string::npos &&
           Read(path).find("object_motion=1\n") != std::string::npos);
    v.canvas_width = v.canvas_height = 0;
    // Native round-trips; a saved output the internal resolution does not offer loads as Native.
    v.output_res = Ps4RtSettings::OutputNative;
    Ps4RtSettings::Save();
    assert(Read(path).find("output_res=native\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.output_res == Ps4RtSettings::OutputNative && v.upscaler == Ps4RtSettings::UpscalerFsr3 &&
           v.object_motion && v.jitter);
    // (This file is at 540p by now: 2560x1440 is offered, 4K is beyond 3x and loads as Native.)
    v.output_res = 2;
    Ps4RtSettings::Save();
    assert(Read(path).find("output_res=2560x1440\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.output_res == 2);
    v.output_res = 3;
    Ps4RtSettings::Save();
    assert(Read(path).find("output_res=3840x2160\n") != std::string::npos);
    Ps4RtSettings::Load(true);
    assert(v.output_res == Ps4RtSettings::OutputNative);
    {
        std::ofstream out(path);
        out << "render_resolution=2560x1080\nrender_resolution_confirmed=2560x1080\noutput_res=3840x2160\n";
    }
    Ps4RtSettings::Load(true);
    assert(v.output_res == Ps4RtSettings::OutputNative);
    {
        std::ofstream out(path);
        out << "render_resolution=2560x1080\nrender_resolution_confirmed=2560x1080\noutput_res=3440x1440\n";
    }
    Ps4RtSettings::Load(true);
    assert(v.output_res == 4);
    Ps4RtSettings::CanvasUnavailable("game code differs at 0x20a5ee");
    assert(v.canvas_problem && !v.canvas_width);
    std::remove(path);
    std::puts("PASS: graphics settings");
}
