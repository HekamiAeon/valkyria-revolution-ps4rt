# Valkyria Revolution — PS4RT for Linux

An experimental, game-specific Linux compatibility runtime for **Valkyria Revolution**
(PlayStation 4, **CUSA06810**), currently tested on **Fedora 44, x86-64**.

PS4RT prepares and runs the game's original x86-64 code on a Linux host, supplies
compatibility implementations of the PS4 services it needs, and uses a Vulkan
graphics renderer derived from shadPS4. This is **not a general-purpose PS4
emulator**, a native recreation of the game, or a replacement for owning the
game.

**No game files, firmware, or decryption tools are supplied.** You must provide
your own extracted, plaintext CUSA06810 game folder. The launcher does not
download games, decrypt encrypted packages, or connect to PSN. This project is
not affiliated with Sega, Sony, or AMD.

## Current status

**Experimental, playable in the tested opening sections; not a verified full
playthrough.** Tested on **Fedora 44** with an **AMD Ryzen 7 9700X** CPU and
**NVIDIA GeForce RTX 3080 Ti** GPU: introduction, cinematics, subtitles, audio,
controller input, menus, early combat, first boss, initial save/load, and 4K
output with FSR 4 v07. Other Linux distributions and hardware configurations
have not yet been verified.

- SDL-compatible gamepads, including DualSense and XInput, plus keyboard controls.
- Output at 1080p, 1440p, or 4K; FSR 3.1, optional FSR 4 v07, and sharpening.
- **Anti-aliasing (AA)** is provided by FSR 3.1 or FSR 4 v07 reconstruction.
  At 1080p, the FSR quality preset is Native AA.
- **Anisotropic filtering (AF)** is separate from AA: select Game default,
  2×, 4×, 8×, or 16× to improve texture detail at an angle. AF changes apply
  after restarting the game. Sharpening is a separate image adjustment.
- In-game PS4RT graphics menu: press **Insert**, or hold **L2 + R3 click**.
  Navigate with the D-pad, confirm with Cross, and go back with Circle.
- **Exit Game** in the graphics menu asks for confirmation, then exits to the
  Linux desktop using the normal window-close path. Save in-game progress first;
  this does not trigger a game save.
- The game renders internally at **1920×1080**. Higher output resolutions are
  reconstructed, **not native 4K rendering**. At 1080p FSR uses Native AA; 1440p
  uses a custom 1.33× ratio; 4K uses Performance scaling.
- Original frame-rate behavior is retained. No 60 FPS patch or frame generation.
- PSN features are unavailable; tested online menu actions returned without a crash.

FSR 3.1 is built in and is the fresh-configuration default. The FSR 4 **v07**
shader/model assets and their MIT notice are included in `fsr4_shaders/`; hashes
and provenance are recorded in `packaging/fsr4-v07-assets.json`. FSR 4 requires
compatible Vulkan GPU features and otherwise falls back to FSR 3.1. This is
**not FSR 4.1.1**.

## Quick start: prebuilt Linux runtime

1. Extract the `valkyria-revolution-linux-x86_64-*.tar.gz` release archive.
2. Open a terminal in the extracted directory.
3. Run the launcher with the path to **your own extracted CUSA06810 folder**:

```bash
bash Play-Valkyria.sh --game /path/to/your/CUSA06810
```

You can also run `bash Play-Valkyria.sh` without `--game` to select the folder
through a graphical chooser, when Python Tkinter is installed. The game folder
must contain `sce_sys/param.sfo`, `eboot.bin`, the required modules, and game
data. The launcher reads the originals and prepares private working files.

**Prebuilt requirements:** Linux x86-64, **glibc 2.43 or later**, Python 3,
a Vulkan loader and compatible graphics driver, and compatible SDL3, FFmpeg,
fmt, xxhash, Zydis, and X11 libraries. System drivers and libraries are **not
bundled**. Fedora 44 is the tested environment; other distributions may need
a source build against their own dependencies.

## Build from source

Install a C/C++ toolchain, CMake, Ninja, pkg-config, Python 3, and development
packages for Vulkan, SDL3, FFmpeg, fmt, Boost, xxhash, robin-map, Vulkan Memory
Allocator, Zydis, glslang, and X11. Third-party sources needed by the build
are vendored in this repository.

From the source checkout's root:

```bash
PS4RT_LTO=OFF bash build.sh --test
python3 scripts/valkyria_launch.py --game /path/to/your/CUSA06810
```

### Keyboard and controller remapping

Default keyboard controls include **WASD** (left stick), **arrow keys** (right
stick), **Space** (Cross), **Left Shift** (Circle), **E** (Square), **Q**
(Triangle), and **I/J/K/L** (D-pad). You can remap PS4 buttons at launch
using `PS4RT_KEY_MAP` without changing source code or recompiling:

```bash
PS4RT_KEY_MAP="cross=Q,triangle=Space" \
  python3 scripts/valkyria_launch.py --game /path/to/your/CUSA06810
```

For the prebuilt archive, use the same variable before
`bash Play-Valkyria.sh --game /path/to/your/CUSA06810`.
`PS4RT_PAD_MAP` similarly remaps gamepad buttons. These are launch-time
environment variables, **not keys in `valkyria.ini`**; there is currently no
in-game key-binding editor. Default mappings and keyboard stick axes are
defined in [`src/runtime_pad.c`](src/runtime_pad.c). Button maps do not
remap the analog stick axes or PS4RT menu shortcut.

See [Running on Linux](docs/RUNNING_LINUX.md) for the full default keyboard
layout, remapping syntax, and controls. For system dependencies, packaging,
and known limitations, see
[Linux distribution guide](docs/DISTRIBUTING_LINUX.md).

### Saves and private files

A source checkout stores generated game code, settings, saves, and GPU caches
under `out/valkyria/`. The packaged launcher uses
`${XDG_DATA_HOME:-~/.local/share}/valkyria-revolution/` by default.
Pass `--data-dir /your/private/path` to use a different directory.

**Never publish these data directories:** generated executables and linked code
still contain proprietary game material. No such content belongs in the
repository or release archives.

## Credits, origins, and license

This Valkyria-specific project is **adapted open-source compatibility software**,
not a clean-room implementation. Its runtime builds on work from:

- [FelipePrado0/gow3_pc](https://github.com/FelipePrado0/gow3_pc), the
  God of War III compatibility-runtime project from which this port was adapted.
- [deadinside28/bloodborne_pc](https://github.com/deadinside28/bloodborne_pc)
  and [Supermedo/bloodborne_pc](https://github.com/Supermedo/bloodborne_pc),
  earlier Bloodborne PC runtime and launcher lineage.
- [shadPS4](https://github.com/shadps4-emu/shadPS4) and related Vulkan
  renderer contributors.

Development also used [REA: Reverse Engineer Anything](https://github.com/morluto/rea)
and [Ghidra](https://github.com/NationalSecurityAgency/ghidra) as
reverse-engineering **analysis tools**, not as embedded runtime code.

See [Upstream credits and notices](docs/UPSTREAM_CREDITS.md) for additional
contributors, source provenance, and third-party licenses. Original authors'
copyright and license notices remain in the source tree. The project's
runtime source is distributed under **GPL-2.0-or-later**; vendored components
retain their respective terms. A separate project name and fresh Git history
do not change those obligations.
