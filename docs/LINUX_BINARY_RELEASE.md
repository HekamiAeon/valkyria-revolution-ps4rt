# Run the Linux x86-64 release (v1.1)

**For players: download, extract, point to your game folder, and play. No compiling. No manual FFmpeg/SDL3 installation.**

## 1. Download the right file

Open the [latest GitHub release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/latest). Download:

- **`valkyria-revolution-linux-x86_64-35f35f5.tar.gz`**: the game runtime for Linux. **Choose this one.**
- `valkyria-revolution-source-35f35f5.tar.gz`: source code for developers, **not needed just to play**.

The earlier v1.0 archive is preserved for historical reference but superseded by v1.1.

## 2. Have your own extracted game

You must have an already extracted, **plaintext** PS4 game directory named `CUSA06810`. It should contain at least:

```text
CUSA06810/
  eboot.bin
  sce_sys/param.sfo
  sce_module/
  (other game data)
```

Do not point the launcher at a PKG, the `eboot.bin` file, or a directory *above* `CUSA06810`. The download does **not** provide the game, firmware, or a PKG extractor.

## 3. Extract and launch

Extract the Linux archive using your file manager, or from a terminal in the directory where you downloaded it:

```bash
tar -xzf valkyria-revolution-linux-x86_64-35f35f5.tar.gz
cd valkyria-revolution-linux-x86_64-35f35f5
bash Play-Valkyria.sh --game "/path/to/CUSA06810"
```

**Replace `/path/to/CUSA06810` with your actual extracted game folder.** Quotation marks are important if your path contains spaces. Keep the extracted Linux release directory intact and writable.

Unlike the source checkout, the packaged launcher puts prepared code, settings and saves under `${XDG_DATA_HOME:-~/.local/share}/valkyria-revolution/` by default. It reads, rather than modifies, your original game extraction. You may choose a different private location with `--data-dir`. You can omit `--game` to select the folder via a graphical chooser if Python Tkinter is installed.

## 4. Choose your graphics settings in the game

**Hold L2 and click R3 (press the right stick inward)** on a controller. On a keyboard, press **Insert**. This opens the PS4RT graphics menu.

Use the **D-pad** to navigate, **Cross** to select, and **Circle** to go back. You can choose:

- Display mode: **Windowed / Borderless / Fullscreen**
- Output resolution: **1080p / 1440p / 4K**
- Upscaling: **Off / FSR 3.1 / FSR 4 v07**, depending on hardware support
- Sharpening and anisotropic filtering (**AF**)

**FSR and resolution commands are unnecessary.** Select them using the wine-red illustrated graphics menu. There is no Sync/Async shader compilation toggle in Valkyria's overlay. The v1.1 runtime includes menu sounds and the portrait-backed panel.

**IMPORTANT: Anisotropic filtering (AF) changes require restarting the game.** Display mode, resolution, upscaling and sharpening apply live.

## Requirements and troubleshooting

- Native x86-64 Linux with **glibc 2.35 or newer**; Fedora has been gameplay-tested, while SteamOS and other distros are unverified. **ARM64, including FEX/Box64 translation, is unsupported.**
- Python 3.
- A working Vulkan loader/graphics driver and host display/input libraries.
- Your own extracted, plaintext `CUSA06810` game.

The release bundles **12 application libraries** including FFmpeg 8 and SDL3, so **do not install those libraries manually for the prebuilt release**. No `LD_LIBRARY_PATH` command is needed. The bundled runtime finds its libraries using relative `$ORIGIN` paths.

Do not use `build.sh` unless you specifically want to compile from source. Developer build instructions are in [`packaging/linux/README.md`](../packaging/linux/README.md). Full controls and known limitations are in [Running on Linux](RUNNING_LINUX.md).

The game is experimental; the first three chapters were tested, but a complete playthrough and compatibility across all distros and GPUs are not verified.
