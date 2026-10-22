# Valkyria Revolution: Linux distribution guide

This experimental native Linux runtime executes code from a player's own
extracted, plaintext **CUSA06810** game. No game is included. The port is based
on earlier compatibility-runtime and shadPS4 code; original copyright and GPL notices remain.

## For players: download, extract, run

**You do not need to compile PS4RT to play.** Once the new portable
v1.1 release is published, download the `valkyria-revolution-linux-x86_64-<commit>.tar.gz`
runtime archive from the [releases page](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases).
The separately named `source` archive is for developers. The old v1.0 public
release predates this glibc 2.35 portable rebuild.

Extract the Linux runtime archive, open a terminal in the extracted folder,
and run:

```bash
bash Play-Valkyria.sh --game /path/to/your/CUSA06810
```

Supply **the extracted CUSA06810 folder**, not the PKG, parent folder or
`eboot.bin` itself. Keep the release files together and writable.

`Play-Valkyria.sh` without arguments opens a folder chooser when Python's Tk
module is installed, then remembers the selected folder. File managers that
support running executable scripts can launch it directly. The extracted
folder must include `eboot.bin`, `sce_sys/param.sfo`, the bundled system modules,
and the game's data directories. The preparer accepts plaintext PS4 SELF files;
it does not decrypt encrypted packages or download a game or firmware.

**Linux x86-64, glibc 2.35 or later. Gameplay-tested on Fedora.**

The portable runtime needs Python 3, glibc 2.35+, a compatible Vulkan loader
and graphics driver, plus the host's display/audio integration libraries.
The archive **bundles twelve application shared libraries** in `lib/`,
including SDL3, FFmpeg 8.0.3, fmt, xxHash, Zydis, zlib, libstdc++, and
libgcc_s. ELF RUNPATHs resolve them relative to the executable, with no
`LD_LIBRARY_PATH` setup or distro-specific installation of those libraries.

System glibc, the Vulkan driver/loader, and system display/audio libraries
remain external. Another Linux distribution may work, but compatibility has
not been established by testing on SteamOS, Mint, or other distributions.
The glibc baseline is a necessary compatibility check, not a universal Linux
guarantee. This is a tar.gz runtime, **not an AppImage**.

Local prepared code, settings and saves go in
`${XDG_DATA_HOME:-~/.local/share}/valkyria-revolution/`.
`--data-dir /some/private/folder` overrides this location. A source checkout
defaults to its existing `out/valkyria/` directory, preserving development saves.
Game input files are only read. The generated `boot*.bin` and linked module
code are still game content: never distribute this private data directory.

Press **Insert**, or hold **L2** and click **R3**, to open the port's menu.
The single graphics panel offers 1080p, 1440p, and 4K output; Off/Original,
FSR 3.1, and FSR 4 v07; anisotropic filtering (Game default / 2× / 4× /
8× / 16×); and sharpening. AF improves angled texture clarity independently
of FSR's anti-aliasing and requires a game restart. Other image changes apply
live. Choices save when the menu closes. The scene
still renders at 1920x1080: 1080p output uses Native AA, 1440p a custom 1.33x ratio,
and 4K Performance scaling. The original game's frame rate is unchanged.
Use the D-pad to navigate, Cross to select, and Circle to back out (A/B on
Xbox controllers).
This overlay is independent of the game's own menu system. Choose **Exit Game**
and confirm **Exit to desktop** for the normal host window-close behavior.
The confirmation warns about unsaved progress; exiting preserves PS4RT graphics
settings but does not automatically save the game.

FSR 3.1 is built in and is the fresh-config default. The 189 checksum-verified
FSR 4 v07 files are bundled in `fsr4_shaders/`, including the MIT notice;
`packaging/fsr4-v07-assets.json` pins their exact size and SHA-256 hashes.
1080p output uses the 1080 tier, while 1440p and 4K output use the 2160 tier.
This is source-v07, not AMD FSR 4.1.1. Unsupported Vulkan GPU features or
initialization failures fall back to FSR 3.1 with a menu message.
There is no automatic network access when launching the game.

## Building from source is for developers

A conventional host-native build still works when the developer installs the
Vulkan, SDL3, FFmpeg, fmt, Boost, xxhash, robin-map, VMA, Zydis, glslang,
X11 and compiler/CMake/Ninja development dependencies:

```bash
PS4RT_LTO=OFF bash build.sh --test
python3 scripts/valkyria_launch.py --game "/path/to/CUSA06810"
```

These binaries use the developer's host libraries. They are **not** the
audited portable release. Use the standardized Ubuntu 22.04 builder
described below for distribution; players do not need Podman.

## Maintainer instructions

### Source and Linux runtime packages

Build the portable runtime from a clean committed source checkout using the
existing `localhost/ps4rt-builder:linux` Podman image. Build the image once
with `packaging/linux/build-images.sh` if it is missing. The builder prints
the exact absolute `BUNDLE=` path. After a gameplay check of that bundle,
the exporter uses those audited binaries instead of any local Fedora build:

```bash
bash packaging/linux/build-runtime.sh
BUNDLE="/absolute/path/from/preceding/BUNDLE/output"
python3 tools/package_valkyria.py --preview > out/source-preview.json
python3 tools/package_valkyria.py
python3 tools/package_valkyria.py --runtime --bundle-from "$BUNDLE" --preview > out/runtime-preview.json
python3 tools/package_valkyria.py --runtime --bundle-from "$BUNDLE"
(cd out/community && sha256sum -c *.sha256)
```

The exporter refuses dirty working trees. It checks the bundled library
allowlist, relative paths, glibc baseline, and library resolution before
building the archive. Extract and gameplay-test the **actual release archive**
before publishing. For developer details, see `packaging/linux/README.md`.

Archives and SHA256 files appear in `out/community/`. The source export contains
tracked files under an explicit set of source directories, including vendored
dependencies, their licenses, build scripts, tests and engineering changes.
The runtime export contains that source under `source/`, three explicitly
named Linux ELF binaries (`bin/ps4rt`, `bin/ps4rt-gpu-capabilities`,
`bin/gpu/libps4rtgpu.so`), twelve allowlisted application shared
libraries in `lib/`, four preparation helpers, the launcher, optional asset
fetch scripts, the 189 bundled FSR 4 v07 assets and notice, this
document and GPL text. `MANIFEST.json` records the revision and SHA256 of
each payload file. Build the binaries from that same source checkpoint.
The package is a local candidate; creating it does not publish or upload it.

The exporter does **not** copy directories from `out/`, originals, saves,
screenshots, shader/pipeline caches, prepared executables, PS4 modules, FSR 4.1.1
DLLs or models. Inherited launcher artwork/font with unreviewed provenance
is excluded from the source export. No new game artwork is needed for the port UI.
Do not share an arbitrary ZIP of this workspace, its Git metadata/history, or
the development checkpoints folder instead of the reviewed export.

Retain matching source and required third-party notices with a binary release.
The port's source retains its GPL-2.0-or-later notices. The Ubuntu builder now
ships twelve shared libraries, including a source-built FFmpeg 8.0.3 with
PNG/zlib support; these are **not** copies of the former Fedora library set.
Audit the precise FFmpeg configuration, component licenses, copyright notices,
and corresponding source/source offers as applicable **before publication**.
The source archive contains vendored upstream notices for FSR-Vulkan/AMD
FidelityFX, ImGui, LibAtrac9, Sirit, half, xbyak, miniz, magic_enum,
AMD register headers, and DejaVu fonts. Review other bundled libraries
separately. See `packaging/linux/THIRD_PARTY_REVIEW.md` in the source export.

Primary license references: [GNU GPLv2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
and [FFmpeg's distribution guidance](https://ffmpeg.org/legal.html).
A game-content-free export is an engineering boundary, not a guarantee about every
jurisdiction's copyright, circumvention or trademark rules. This document does
not grant permission to distribute Sega/Sony game content or use their branding.

Before a public release: test the extracted candidate on the declared target,
verify the checksums/file list, provide its matching source archive and attribution,
and describe the actual tested gameplay coverage and known limitations.
**The bundled shared-library notices and corresponding-source checklist is
not complete; do not publish v1.1 until it has been resolved.** Publication
and uploading remain a separate owner decision.
