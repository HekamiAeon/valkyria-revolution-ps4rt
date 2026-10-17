// ps4rt.ini graphics keys (ps4rt_settings.cpp, ps4rt_graphics.h).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
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
    assert(G::ParseResolution("1440p") == 3 && G::ParseResolution("8K") == 0);
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
               "render_resolution=1440p\nengine_fps=240\ndeferred_readback=0\nstale_readback=1\n"
               "fsr1=1\nrcas=0\nrcas_strength=40\nframes_queued=2\n";
    }
    Env("PS4RT_CONFIG", path);
    Env("PS4RT_ASYNC_SHADERS", "");
    auto& v = Ps4RtSettings::Get();
    Ps4RtSettings::Load();
    assert(v.display_mode == G::Borderless && !v.vsync && v.fps_limit == 240 && !v.async_shaders);
    assert(v.render_resolution == 3 && v.engine_fps == 240 && !v.deferred_readback && v.stale_readback);
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
                             "render_resolution=1440p\n", "engine_fps=240\n", "deferred_readback=0\n",
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
    Ps4RtSettings::Load(true);
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
    std::remove(path);
    std::puts("PASS: graphics settings");
}
