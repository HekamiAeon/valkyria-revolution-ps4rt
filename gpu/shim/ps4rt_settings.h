// SPDX-License-Identifier: GPL-2.0-or-later
// ps4rt: user settings changed at run time from the in-game menu (ps4rt_overlay.h) and kept
// in ps4rt.ini (PS4RT_CONFIG overrides the path). Environment variables override the file at
// start. Readers load the atomics every frame; writers are the menu and Load().

#pragma once


#include <atomic>
#include <cstdint>

namespace Ps4RtSettings {

enum Upscaler : int { UpscalerOff = 0, UpscalerFsr3 = 1, UpscalerFsr4 = 2, UpscalerFsr411 = 3,
                      UpscalerTaa = 4, UpscalerDlss = 5,
                      /* Spatial FSR 1 on the scene before the HUD (no motion, no history). */
                      UpscalerFsr1 = 6, UpscalerCount };
/// FSR 4 v07 or FSR 4.1.1: the same inputs, settings and placement in the frame.
inline bool IsFsr4(int upscaler) {
    return upscaler == UpscalerFsr4 || upscaler == UpscalerFsr411;
}
enum Preset : int { NativeAA = 0, Quality, Balanced, Performance, UltraPerformance, PresetCount };
enum DebugView : int { DebugNone = 0, DebugReactive = 1, DebugMotion = 2, DebugViewCount };

/// FSR output resolutions (live): the upscaler's output and the HUD's host targets (the Sakura
/// Wars / Code Fairy list). 0 is Native: FSR anti-aliasing at the internal resolution.
inline constexpr int OutputWidths[] = {0, 1920, 2560, 3840, 3440, 3840, 5120};
inline constexpr int OutputHeights[] = {0, 1080, 1440, 2160, 1440, 1440, 1440};
inline constexpr int OutputCount = 7;
inline constexpr int OutputNative = 0;
inline constexpr int OutputDefault = OutputNative;

/// An output can be chosen for an internal resolution when it is larger in both dimensions, at
/// most 3x (FSR's range), and has the same aspect within 1% (2560x1080 -> 3440x1440 is the only
/// inexact pair). Native always.
bool OutputAllowed(int internal_width, int internal_height, int output);

struct Values {
    bool valkyria = false; ///< Game profile, fixed before renderer/window threads start.
    std::atomic<bool> spatial_aa{false}; ///< Final game image AA before spatial upscaling.
    std::atomic<int> upscaler{UpscalerFsr3};
    std::atomic<int> preset{NativeAA};
    std::atomic<bool> sharpen{true};
    std::atomic<float> sharpness{0.3f};
    std::atomic<bool> jitter{true};
    std::atomic<bool> reactive{false};
    std::atomic<bool> object_motion{true};
    std::atomic<float> reactive_scale{1.0f};
    std::atomic<float> reactive_threshold{0.2f};
    std::atomic<float> reactive_max{0.9f};
    std::atomic<int> debug_view{DebugNone};
    std::atomic<bool> show_fps{false};
    /// Performance overlay (ps4rt_perf_stats.h): items shown, font size and background opacity
    /// in percent, corner (0 top left, 1 top right, 2 bottom left, 3 bottom right), layout
    /// (0 one line, 1 one item per line). menu_tab: the menu's last tab.
    std::atomic<unsigned> overlay_items{7};
    std::atomic<int> overlay_scale{100}, overlay_corner{1}, overlay_opacity{50}, overlay_layout{0};
    std::atomic<int> menu_tab{0};
    // Graphics (ps4rt_graphics.h). Live: applied on the next frame.
    std::atomic<int> display_mode{0};
    std::atomic<bool> vsync{true};
    std::atomic<int> fps_limit{0};
    std::atomic<bool> async_shaders{true};
    std::atomic<bool> fsr1{false};
    std::atomic<bool> rcas{true};
    std::atomic<int> rcas_strength{75};
    /// Set by the presenter: sharpening ran on the last frame (it needs FSR 1 when the game's
    /// image is smaller than the window).
    std::atomic<bool> rcas_applied{false};
    /// Set by the presenter: the game's image and the window it is shown in, in pixels.
    std::atomic<int> image_width{0}, image_height{0}, window_width{0}, window_height{0};
    std::atomic<int> frames_queued{1};
    /// Controller vibration strength in percent; 0 turns it off (runtime_pad.c). Live.
    std::atomic<int> vibration{100};
    // Startup: run.py turns them into patches and environment variables; "Apply and restart".
    std::atomic<int> render_resolution{0};
    std::atomic<int> engine_fps{120};
    std::atomic<bool> deferred_readback{true};
    std::atomic<bool> stale_readback{true};
    /// Ps4RtGraphics::Anisotropies; the samplers read startup_anisotropy.
    std::atomic<int> anisotropy{0};
    int startup_anisotropy = 0;
    /// Set before an "Apply and restart"; cleared once the new launch shows the game. run.py
    /// restores the previous startup options when a launch never cleared it.
    std::atomic<bool> restart_unconfirmed{false};
    int startup_render_resolution = 0, startup_engine_fps = 120;
    /// The engine rate this launch really runs at (run.py's PS4RT_ENGINE_FPS): 60 when the frame
    /// rate patch does not apply to this game build.
    std::atomic<int> applied_engine_fps{120};
    bool startup_deferred_readback = true, startup_stale_readback = true;
    /// The last render_resolution that reached the game (render_resolution_confirmed). A launch
    /// with another one marks restart_unconfirmed first; when the next launch still finds the
    /// mark, the previous one never reached the game and the confirmed resolution returns.
    int confirmed_render_resolution = 0;
    /// The native canvas this launch renders at (Valkyria startup patches): any internal
    /// resolution other than the game's own 1920x1080, else 0.
    /// Set by ps4rtgpu_init before the renderer threads start.
    int canvas_width = 0, canvas_height = 0;
    /// Why the selected canvas did not start (this launch runs at 1920 x 1080), or null.
    const char* canvas_problem = nullptr;
    /// Valkyria engine frame rate (valkyria_frame_rate.h): 60 updates a second (experimental)
    /// instead of the game's 30. Live while the frame rate patches are loaded (SixtyFpsLive);
    /// startup_sixty_fps is the rate the launch began with.
    std::atomic<bool> sixty_fps{false};
    bool startup_sixty_fps = false;
    /// Settings file only: 0 starts the unmodified game code, at 30 only.
    bool frame_rate_patches = true;
    /// Why the option did not start (this launch runs at the game's 30), or null.
    const char* sixty_fps_problem = nullptr;
    /// Valkyria graphics quality (Ps4RtGraphics::Quality, valkyria_quality.h): High and Low
    /// reduce the game's shadows for slow GPUs. Startup patches, so a change applies at the
    /// next launch; startup_graphics_quality is what this launch began with.
    std::atomic<int> graphics_quality{0};
    int startup_graphics_quality = 0;
    /// Why the selected quality did not start (this launch runs at Max), or null.
    const char* graphics_quality_problem = nullptr;
    // FSR 4 checks (menu): the provider's auto exposure, the jitter sign it is given.
    std::atomic<bool> fsr4_auto_exposure{true};
    std::atomic<bool> fsr4_invert_jitter{false};
    std::atomic<int> active_render_width{1920}, active_render_height{1080};
    std::atomic<int> output_res{OutputDefault}; ///< index into OutputWidths
    /// Live resolution and preset changes (run.sh): 0 off by default (startup patch, fastest
    /// on the Steam Deck and older GPUs), -1 auto (strong discrete GPUs), 1 on. On restart.
    std::atomic<int> live_resolution{0};
    /// Why FSR 4 cannot run (assets, device features), or null. Set by the renderer.
    std::atomic<const char*> fsr4_problem{nullptr};
    std::atomic<bool> fsr4_supported{false}, fsr411_supported{false};
    /// DLSS (ps4rt_dlss.dll, NVIDIA RTX) is ready, or why not (null before the device exists).
    std::atomic<bool> dlss_supported{false};
    std::atomic<const char*> dlss_problem{nullptr};

    /// Startup settings for the explicit PS4RT_RENDER_RES compatibility patch only.
    int startup_preset = NativeAA;
    int startup_upscaler = UpscalerFsr3;
    bool startup_object_motion = true;
    int startup_output_res = OutputDefault;
    int startup_live_resolution = 0;
};

Values& Get();

/// Reads the file, then the environment overrides. Called once at start.
void Load(bool valkyria = false);
/// Startup override; null preserves the saved choice. Called after Load's file parsing.
void ConfigureValkyriaUpscaler(const char* requested, bool object_motion);
/// Live menu/provider transition, including the required motion/jitter/filter state.
void SetValkyriaUpscaler(int selected, bool object_motion = true);
/// Checks the loaded choice before the first frame; unsupported FSR 4 uses FSR 3.1.
void ConfigureUpscalerSupport(bool fsr4, bool fsr411);
/// After device creation: DLSS availability; a DLSS setting falls back to FSR 3.1 without it.
void ConfigureDlssSupport(bool available, const char* problem);
/// Startup-patched scene dimensions cannot change until run.sh prepares a new image.
bool FixedRenderSession();
int RenderPreset();
bool ResolutionNeedsRestart();
/// A startup graphics option differs from the one this launch started with.
bool GraphicsNeedRestart();
/// The canvas started: the scene is width x height (the FSR output follows output_res).
void ActivateCanvas(int width, int height);
/// The selected canvas could not start (static text): this launch stays 1920 x 1080.
void CanvasUnavailable(const char* problem);
/// The 60 FPS option could not start (static text): this launch stays at the game's 30.
void SixtyFpsUnavailable(const char* problem);
/// Valkyria: this launch has the frame rate patches, so the engine frame rate switches live.
bool SixtyFpsLive();
/// The scene size this launch renders at: the canvas, else 1920 x 1080.
void SceneSize(int& width, int& height);
/// The FSR output and host image for an OutputWidths index: the output fitted to the scene
/// aspect (never stretched; 2560x1080 -> 3414x1440), or the scene size for Native or an output
/// OutputAllowed rejects for this scene.
void OutputSize(int output, int& width, int& height);
/// After about ten seconds of game frames: this launch's render resolution is confirmed.
void ConfirmRenderResolution();
/// Writes the file (menu changes).
void Save();

/// Render resolution divisor of a preset (1.0 native, 1.5 quality, ...).
float PresetScale(int preset);
const char* PresetName(int preset);
const char* UpscalerName(int upscaler);

} // namespace Ps4RtSettings
