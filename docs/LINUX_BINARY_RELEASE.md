# Run the Valkyria Revolution PS4RT Linux x86-64 release (v1.3)

**For players: download the portable Linux runtime, extract it, and point the launcher at your own game. No compilation or manual FFmpeg/SDL3 installation is required.**

## 1. Download the right file

Open the [Valkyria Revolution PS4RT v1.3 GitHub release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/tag/v1.3)
(or [the latest release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/latest)). Download:

- **[valkyria-revolution-linux-x86_64-a901bd2.tar.gz](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/download/v1.3/valkyria-revolution-linux-x86_64-a901bd2.tar.gz)**: prebuilt Linux runtime. **Choose this to play.**
- [valkyria-revolution-linux-x86_64-a901bd2.tar.gz.sha256](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/download/v1.3/valkyria-revolution-linux-x86_64-a901bd2.tar.gz.sha256): SHA256 verification file for the runtime.
- [valkyria-revolution-source-a901bd2.tar.gz](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/download/v1.3/valkyria-revolution-source-a901bd2.tar.gz): matching source code for developers, **not required just to play**.
- [valkyria-revolution-source-a901bd2.tar.gz.sha256](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/download/v1.3/valkyria-revolution-source-a901bd2.tar.gz.sha256): SHA256 verification file for the source archive.

These are the four uploaded release assets. GitHub's automatic “Source code”
ZIP/tarball links are different from the matching source package above.

To verify your runtime download, save the Linux `.tar.gz` and its `.sha256`
in the same directory and run:

```bash
sha256sum -c valkyria-revolution-linux-x86_64-a901bd2.tar.gz.sha256
```

It should report `OK`.

## 2. Supply your own extracted PS4 game

The game is **Valkyria Revolution, title ID CUSA06810**. Supply an extracted, *plaintext* game directory with a structure like:

```text
CUSA06810/
  eboot.bin
  sce_sys/param.sfo
  sce_module/
  (other game data)
```

Point the launcher to `CUSA06810` itself, **not** a PKG, `eboot.bin`, or its parent directory.

Neither the runtime nor the source archive includes PS4 game data, updates, DLC, firmware, decryption keys, or saved games.

## 3. Extract and launch

Extract the Linux archive using your file manager, or in the directory
containing the downloaded runtime:

```bash
tar -xzf valkyria-revolution-linux-x86_64-a901bd2.tar.gz
cd valkyria-revolution-linux-x86_64-a901bd2
bash Play-Game.sh --game "/path/to/CUSA06810"
```

Replace the quoted path with the location of **your own extracted game folder**. Keep the entire extracted PS4RT directory together. Prepared game files, settings, saves and shader caches live under `~/.local/share/valkyria-revolution/` (`--data-dir` selects another folder); your extracted game folder is read, not modified.

**Later launches:** After successful first-time preparation, run `bash Play-Game.sh` without `--game`. The player remembers the validated CUSA06810 directory. Supply `--game` again if it moves. With no configured game path, the launcher prints an example command and directs you to `PLAYER_README.txt` in the extracted player folder.

## Requirements and limitations

- Native **x86-64/AMD64 Linux**, with **glibc 2.35+** and **Python 3**. ARM64 and CPU translators such as FEX or Box64 are not supported or validated, they might or might not work depending on the device.
- Working Vulkan 1.3-capable graphics driver/loader and compatible desktop display/input environment. FSR 4 depends on additional compatible device features and falls back to other supported options when unavailable.
- Fedora Linux with AMD/RADV/NVIDIA graphics has been gameplay-tested.
- The portable bundles its audited application libraries, including SDL3 and FFmpeg. **Do not install separate versions or set `LD_LIBRARY_PATH` merely to run the portable.**
