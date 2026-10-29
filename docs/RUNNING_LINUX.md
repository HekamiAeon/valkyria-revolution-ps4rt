# Running Valkyria Revolution on Linux

This is an experimental, game-specific native x86-64 Linux compatibility runtime for the PlayStation 4 version of **Valkyria Revolution (CUSA06810)**. PS4RT executes prepared game code with PS4 service compatibility shims and a shadPS4-derived Vulkan renderer. It is **not a general-purpose PS4 emulator**.

You must supply your own **extracted, plaintext** CUSA06810 game folder. Neither this repository nor the launcher includes the game, PS4 firmware, package decryption tools, or downloads.

## Download and run v1.2 (no compiling)

**Playing the game? Download the Linux runtime archive from the [latest release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/tag/v1.2).** Choose `valkyria-revolution-linux-x86_64-d29d542.tar.gz`, not the source archive.

1. Have your own extracted, plaintext `CUSA06810` directory. It must contain `sce_sys/param.sfo`, `eboot.bin`, `sce_module/`, and the game's data. Do not point to a `.pkg`, `eboot.bin` alone, or the parent of `CUSA06810`.
2. Extract the downloaded `.tar.gz` with your file manager, or run the extraction commands below.
3. Open a terminal **inside the extracted Linux release folder**, then run `bash Play-Game.sh --game "/path/to/CUSA06810"`. Replace the example path with your real one and keep quotes if it contains spaces.

```bash
tar -xzf valkyria-revolution-linux-x86_64-d29d542.tar.gz
cd valkyria-revolution-linux-x86_64-d29d542
bash Play-Game.sh --game "/path/to/CUSA06810"
```

After the first successful launch, run `bash Play-Game.sh` without `--game` to reuse the remembered game folder. Supplying `--game` again overrides it. If no valid location is configured, the launcher gives an example command and directs you to `PLAYER_README.txt`. No Tkinter picker is provided.

**No library installation step is necessary for FFmpeg, SDL3, fmt, Zydis, or the other bundled application libraries.** No `LD_LIBRARY_PATH` setting or source compilation is required. You still need x86-64 Linux, glibc 2.35+, Python 3, and a working host Vulkan graphics/display stack.

### Graphics: choose settings inside the game

**Hold L2 and click R3 (press the right stick inward)** on a controller, or press **Insert** on the keyboard to open the PS4RT graphics menu. Set **Windowed / Borderless / Fullscreen** display mode, **1080p, 1440p or 4K** output, **Off / FSR 3.1 / FSR 4 v07** upscaling (as supported), sharpening and anisotropic filtering (AF). D-pad moves, Cross confirms, Circle goes back. The illustrated wine-red overlay plays menu navigation, confirm and cancel sounds.

**FSR and resolution command-line arguments are not required.** Choose graphics settings inside the menu. There is no Sync/Async shader compilation toggle in this game's overlay.

**IMPORTANT: AF (anisotropic filtering) changes require a game restart to take effect.** Display mode, resolution, upscaling and sharpening apply live. The menu saves changed settings when it closes.

The game is not included in this download. Fedora has been gameplay-tested; SteamOS and other distributions still need independent verification. See [Linux binary release](LINUX_BINARY_RELEASE.md) for the package layout.

## Building from source (developers only)

Install the source dependencies listed in the [README](../README.md), including a C/C++ toolchain, Python 3, CMake, Ninja, pkg-config, Vulkan, SDL3, FFmpeg and the other CMake dependencies. Fedora Linux x86-64 is the tested environment.

From the repository root, build the runtime and run its automated checks:

~~~bash
PS4RT_LTO=OFF bash build.sh --test
~~~

A faster subsequent build, without rerunning all tests:

~~~bash
PS4RT_LTO=OFF bash build.sh
~~~

The runtime binary is generated at `out/ps4rt`. Passing the test suite checks the build, **not** every part of the game.

## Launching a source build (developers only)

Point the source launcher at your own extracted `CUSA06810` game directory:

~~~bash
python3 scripts/valkyria_launch.py --game "/path/to/CUSA06810"
~~~

The game folder must include `sce_sys/param.sfo` with the appropriate title ID, `eboot.bin`, the required modules and game data. The input must already be extracted and usable; the launcher does not decrypt encrypted game packages.

The source launcher also remembers the chosen directory. Subsequent runs may omit `--game`. A Tkinter folder picker is not available. You can prepare files without launching the game:

~~~bash
python3 scripts/valkyria_launch.py --game "/path/to/CUSA06810" --prepare-only
~~~

### Local files, saves, and shader caches

A source checkout stores its generated working files under `out/valkyria/`. A downloaded release instead stores private data under `${XDG_DATA_HOME:-~/.local/share}/valkyria-revolution/` by default. The optional `--data-dir` flag selects another directory.

| Location | Purpose |
| --- | --- |
| `out/valkyria/valkyria.ini` | Saved graphics preferences in a source checkout |
| `out/valkyria/user/savedata/` | In-game saves for a source checkout |
| `out/valkyria/user/gpu/` | Generated game shaders and Vulkan pipeline caches, when present |
| `out/valkyria/boot-linked.bin` | Prepared game-linked executable data |

**Never publish or commit `out/` or the packaged player's user-data directory.** Prepared code contains game material, and saves/caches are private user data. Game inputs are read from the supplied extracted folder rather than distributed with the runtime.

## Controls

SDL-compatible controllers, including DualSense and XInput pads, and keyboard input are supported. Keyboard defaults apply while the game has focus:

| PS4 control | Keyboard |
| --- | --- |
| Cross / confirm | Space |
| Circle / back | Left Shift |
| Square / Triangle | E / Q |
| Options | Enter |
| D-pad up / down / left / right | I / K / J / L |
| Left stick | W / S / A / D |
| Right stick | Arrow keys |
| L1 / R1 | 1 / 3 |
| L2 / R2 | R / F |
| L3 / R3 | Z / C |
| Left / right touchpad click | Tab / Backspace |

The two touchpad-click actions are mapped independently; their behavior depends on the game.

### Remapping keyboard or gamepad buttons

The default mappings and parsing logic are in [`src/runtime_pad.c`](../src/runtime_pad.c). For custom button mappings, you **do not** need to edit that file or rebuild. Set `PS4RT_KEY_MAP` on the launch command:

~~~bash
PS4RT_KEY_MAP="cross=Q,triangle=Space" \
  python3 scripts/valkyria_launch.py --game /path/to/your/CUSA06810
~~~

Entries are comma-separated `ps4_button=SDL key name` pairs. Supported button names include `cross`, `circle`, `square`, `triangle`, `l1`, `r1`, `l2`, `r2`, `l3`, `r3`, `options`, `up`, `down`, `left`, `right`, `touchpad_left` (or `touchpad`), and `touchpad_right`.

`PS4RT_PAD_MAP` uses SDL gamepad button names with the same syntax:

~~~bash
PS4RT_PAD_MAP="cross=b,circle=a" \
  python3 scripts/valkyria_launch.py --game /path/to/your/CUSA06810
~~~

Both mapping variables take effect at launch, **not** in the PS4RT settings file. They do not remap analog stick axes, physical triggers, or the Insert / L2+R3 overlay shortcut. There is no in-game key-binding editor.

### Graphics overlay and exiting

Press **Insert**, or hold **L2** and click **R3**, to open the custom Valkyria PS4RT graphics menu. Use the D-pad to navigate, Cross to select, and Circle to go back. The overlay offers:

- Display mode: **Windowed / Borderless / Fullscreen**.
- Output resolution: **1080p / 1440p / 4K**.
- Upscaling: **Off / Original / FSR 3.1 / FSR 4 v07** when supported.
- Sharpening and anisotropic filtering (**Game default / 2× / 4× / 8× / 16×**).

Resolution, display mode, upscaler selection and sharpening apply live; **IMPORTANT: changing anisotropic filtering (AF) requires restarting the game**. The overlay saves changed settings when you close it. The game renders its original 1080p scene before display reconstruction, so a 4K output is not native 4K game rendering. There is no bundled 60 FPS patch or frame-generation mode. The portrait-backed wine-red menu includes recorded navigation, confirm and cancel sounds.

To restart, choose **Restart Game** and confirm. To quit, choose **Exit Game**, then confirm **Exit to desktop**. Cancel or Circle/Escape returns to the game. **Save progress inside Valkyria before exiting**; saving PS4RT graphics settings is not a game save.

FSR 3.1 is part of the runtime. The FSR 4 v07 shader/model files and license notice are in `fsr4_shaders/`; they are required renderer dependencies, **not** game shader cache files. FSR 4 needs compatible Vulkan GPU features. If it is unavailable, use FSR 3.1. See the [Linux distribution guide](DISTRIBUTING_LINUX.md).

## Compatibility and limitations

Gameplay has been verified through the **opening three chapters**, including the introduction, cinematics, audio, menus, controller input, exploration, early combat and initial saves. Fedora development testing used an AMD Ryzen 7 9700X and NVIDIA GeForce RTX 3080 Ti; the portable release received an additional gameplay and overlay smoke test on an AMD-based Fedora system. 4K output with FSR 4 v07 has also been tested.

**A complete playthrough and broad compatibility across Linux distributions and hardware are not verified.** PSN functionality is not provided. Other games and PS4 title IDs are outside this game-specific release.
