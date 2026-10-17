# Running Valkyria Revolution on Linux

This is an experimental, standalone Linux compatibility runtime for the PlayStation 4 version of Valkyria Revolution (CUSA06810). It requires user-supplied game files; no game or firmware is distributed.

## Building

Install the development dependencies listed in the [README](../README.md), then run:

```bash
PS4RT_LTO=OFF bash build.sh --test
```

## Launching

Point the launcher at your own extracted, plaintext CUSA06810 game folder:

```bash
python3 scripts/valkyria_launch.py --game /path/to/CUSA06810
```

Without `--game`, the launcher can offer a folder chooser when Tkinter is available. The selected folder must include `sce_sys/param.sfo` and game executable/data files. The launcher does not decrypt packages or download game files.

Build outputs, prepared code, saves, and settings are stored under `out/valkyria/` for source builds. A packaged runtime uses `${XDG_DATA_HOME:-~/.local/share}/valkyria-revolution/`. These local directories must not be published.

## Controls

SDL-compatible controllers (including DualSense and XInput pads) and keyboard input are supported. Keyboard controls while the game window has focus:

| PS4 control | Keyboard |
| --- | --- |
| Cross / confirm | Space |
| Circle / back | Left Shift |
| Square / Triangle | E / Q |
| Options | Enter |
| D-pad | I / K / J / L |
| Left stick | W / S / A / D |
| Right stick | Arrow keys |
| L1 / R1 | 1 / 3 |
| L2 / R2 | R / F |
| L3 / R3 | Z / C |
| Left / right touchpad click | Tab / Backspace |

### Remapping keyboard or controller buttons

The mappings are implemented in [`src/runtime_pad.c`](../src/runtime_pad.c).
To customize PS4 buttons, **you do not need to edit that file or rebuild**.
Set the `PS4RT_KEY_MAP` environment variable when launching. Entries use
`ps4_button=SDL key name`, separated by commas, and override the default
source key for the named PS4 button. Example: swap Cross and Triangle:

```bash
PS4RT_KEY_MAP="cross=Q,triangle=Space" \
  python3 scripts/valkyria_launch.py --game /path/to/CUSA06810
```

For a prebuilt archive, use the same variable with
`bash Play-Valkyria.sh --game /path/to/CUSA06810`. Available PS4 button
names are `cross`, `circle`, `square`, `triangle`, `l1`, `r1`,
`l2`, `r2`, `l3`, `r3`, `options`, `up`, `down`, `left`,
`right`, `touchpad_left` (or `touchpad`), and `touchpad_right`.
Use SDL key names such as `Space`, `Q`, `Left Shift`, or `Left Ctrl`.

`PS4RT_PAD_MAP` uses the same syntax with SDL gamepad button names, for
example `PS4RT_PAD_MAP="cross=b,circle=a"`. The mappings apply when the game
starts; they are **not stored in `valkyria.ini`**, and there is no in-game
binding editor. The keyboard's WASD and arrow-key stick axes, physical
gamepad sticks/triggers, and the Insert/L2+R3 overlay shortcut are not
remapped by these variables. Changing those fixed input controls requires
editing the relevant source and rebuilding.

### Graphics menu and exiting

Press **Insert** or hold **L2** and click **R3** to open the runtime graphics menu. Use D-pad, Cross, and Circle to navigate. Display output choices include 1080p, 1440p and 4K, with FSR 3.1 or FSR 4 v07 on compatible GPUs, and sharpening. FSR 3.1 and FSR 4 v07 provide anti-aliasing (AA) as part of reconstruction, including Native AA at 1080p. Anisotropic filtering (AF) is a separate texture option with Game default / 2× / 4× / 8× / 16× choices. AF changes are saved when closing the menu and take effect after restarting the game; resolution, FSR and sharpening apply live. The game still renders its original 1080p scene before reconstruction; a 4K output is not native 4K rasterization. The game's original frame-rate behavior is retained.

To leave the game, open the graphics menu and choose **Exit Game**, then
confirm **Exit to desktop**. Select **Cancel** or press Circle/Escape to stay
in the game. Runtime graphics settings are saved before exiting, but this
does not save your in-game progress; use the game's save feature first.

FSR 3.1 and the checksum-verified FSR 4 v07 assets are included. FSR 4 v07
requires compatible Vulkan GPU features; see [distribution guidance](DISTRIBUTING_LINUX.md).
If unsupported or unavailable, use FSR 3.1.

## Compatibility and limitations

The introduction, audio, subtitles, controller input, early combat, first boss, and initial save/load have been tested on Fedora 44. A complete playthrough and operation on other distributions/hardware have **not** been verified. PSN features are unavailable. The in-game save interface is provided by the compatibility runtime.

PS4RT is the reusable compatibility runtime; Valkyria Revolution (CUSA06810) is
the game-specific adapter in this repository. No files from other games are required.
