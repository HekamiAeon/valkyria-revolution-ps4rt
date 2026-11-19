# Running Valkyria Revolution PS4RT on Linux

Valkyria Revolution PS4RT is an experimental, game-specific x86-64 Linux compatibility runtime for **CUSA06810**. No game files are distributed. Players should use the prebuilt [v1.3 release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/tag/v1.3); compiling from source is for developers.

## 1. Download and run the portable

1. Download **`valkyria-revolution-linux-x86_64-a901bd2.tar.gz`** from the [v1.3 release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/tag/v1.3), not the source archive. Extract it into a writable directory.
2. Provide your own extracted, plaintext **`CUSA06810`** directory containing `eboot.bin`, `sce_sys/param.sfo`, `sce_module/` and game data. Point to that directory itself, not the PKG, its parent, or the executable.
3. Open a terminal inside the extracted player directory and run:

```bash
bash Play-Game.sh --game "/absolute/path/to/CUSA06810"
```

**Later launches:** After successful preparation, run `bash Play-Game.sh` alone. The player remembers the validated path in `~/.local/share/valkyria-revolution/game-folder.txt`. Use `--game` again if you move the extraction or select another copy. With no configured path, the launcher prints a first-launch command and points to `PLAYER_README.txt`.

## 2. Graphics and controls

Open PS4RT settings with **Insert** on a keyboard or **L2 + R3 click** on a
gamepad. Navigate with the D-pad, confirm with Cross, and go back with Circle.
The full overlay includes **Achievements**; the compact overlay has Graphics
and Achievements tabs (switch with L1/R1 or Q/E).

The current source's **Graphics** menu includes:

- **Internal resolution:** 1024×576 (low-end "540p"), 1280×720, 1920×1080,
  2560×1440, and 3840×2160, plus experimental **21:9** (2560×1080,
  3440×1440), **24:9** (3840×1440) and **32:9** (3840×1080, 5120×1440).
  **Restart required** when changing the internal resolution.
- **FSR Upscale output:** A separate choice from internal resolution. Only
  higher outputs within the same aspect-ratio family and the supported scaling
  range are offered; **Native** keeps the internal dimensions.
- **Upscaling:** **Off / FSR 1 / FSR 3.1 / FSR 4 v07**, depending on GPU
  support. FSR 1 uses spatial scaling without temporal anti-aliasing; FSR 3.1
  and FSR 4 use temporal reconstruction. The 3D scene is reconstructed before
  the HUD is composited. FSR selection and output changes apply live.
- **Graphics quality:** Max (original), High, and Low. High draws
  lower-resolution shadows; Low disables shadows. **Restart required** when
  changing the graphics quality.
- **Other controls:** Sharpening, anisotropic filtering, display mode, VSync,
  FPS/performance overlay, and original 30 FPS or experimental 60 FPS.
  **Anisotropic filtering requires a restart.**

**Ultrawide is experimental.** Some HUD markers, dialogue UI, 2D screens and
cutscenes may remain framed for 16:9. A higher FSR output resolution does not
mean the scene itself is rendered at that resolution. Compare visual issues
against 16:9 with FSR Off.

**Local achievements:** The game triggers trophy unlocks; the **Achievements**
tab shows local progress, icons, hidden entries, unlock dates and notifications.
Progress is saved locally and **not synchronized with PSN**. Previously
completed objectives will not unlock retroactively.

FSR 4 requires compatible Vulkan GPU features, including INT8 dot-product
support; use **FSR 3.1, FSR 1, or Off** on unsupported hardware. Choose
internal resolution, FSR output, and upscaler through the in-game overlay.

**Save your game before using Restart Game** or quitting, since graphics settings and game progress are saved separately.

## 3. Settings, saves, and private data

Keep the extracted player directory together. Generated files and player data live under `~/.local/share/valkyria-revolution/` (or `$XDG_DATA_HOME/valkyria-revolution/`); `--data-dir` selects another folder:

- `~/.local/share/valkyria-revolution/game-folder.txt`: remembered game directory.
- `~/.local/share/valkyria-revolution/valkyria.ini`: graphics settings.
- `~/.local/share/valkyria-revolution/user/savedata/`: game saves.
- `~/.local/share/valkyria-revolution/user/achievements/`: locally earned trophy state and
  generated catalog.
- `~/.local/share/valkyria-revolution/user/gpu/cache/`: shader/pipeline caches; deleting a cache forces recompilation.

**Never redistribute this folder**: it may contain game-derived code, saves, caches and private player data.

## 4. Build from source (developers only)

A source checkout requires CMake 3.24+, Ninja, a C11/C++23 compiler, pkg-config, Python 3, glslang, and development packages for Vulkan, SDL3/X11, fmt, Boost, tsl-robin-map, xxHash, Zydis and FFmpeg. Package names differ by distribution. Players of the prebuilt archive need none of these developer tools or a special `LD_LIBRARY_PATH`.

```bash
PS4RT_LTO=OFF bash build.sh --test
python3 scripts/valkyria_launch.py --game "/absolute/path/to/CUSA06810"
```

Source builds use `out/ps4rt` and keep their data under `out/valkyria/`; the portable uses `bin/ps4rt`. For redistributable archives, follow the audited [portable build instructions](../packaging/linux/README.md) and [distribution guide](DISTRIBUTING_LINUX.md).

## 5. Compatibility and troubleshooting

- **System:** Native Linux x86-64/AMD64, glibc 2.35+, Python 3, Vulkan 1.3-capable drivers and compatible host display/input libraries. ARM64 and CPU translation via FEX or Box64 are unsupported/unverified.
- **Testing:** Fedora gameplay including saves through Chapters 1–4 has been verified; a complete playthrough, other distributions and SteamOS have not been exhaustively tested. The 60 FPS mode remains experimental.
- **DrawPipeline:** Off by default. On CPU-limited systems, try `PS4RT_DRAW_PIPE=1 bash Play-Game.sh`. It may cause flickering or missing UI/fade draws; reproduce bugs with DrawPipeline off before reporting them.
- **No configured game folder:** Supply `--game "/absolute/path/to/CUSA06810"` on first launch, or again if you moved the folder; consult `PLAYER_README.txt`.
- **FSR 4 unavailable:** Your GPU/driver may lack the required INT8 dot-product features. Use FSR 3.1, FSR 1, or Off instead.
- **Ultrawide UI misplaced:** Compare the same scene at 16:9 with FSR Off; record internal resolution, FSR output, upscaler, scene, and a screenshot.
- **Source checkout says runtime missing:** Run `bash build.sh`, or download the prebuilt player archive.

For bug reports and feature notes, see the [main README](../README.md) or [GitHub Issues](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/issues).
