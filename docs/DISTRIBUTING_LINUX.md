# Valkyria Revolution: community package

This experimental native Linux runtime executes code from a player's own
extracted, plaintext **CUSA06810** game. No game is included. The port is based
on earlier compatibility-runtime and shadPS4 code; original copyright and GPL notices remain.

## Player instructions

Extract the runtime archive and run:

```bash
bash Play-Valkyria.sh --game /path/to/your/CUSA06810
```

`Play-Valkyria.sh` without arguments opens a folder chooser when Python's Tk
module is installed, then remembers the selected folder. File managers that
support running executable scripts can launch it directly. The extracted
folder must include `eboot.bin`, `sce_sys/param.sfo`, the bundled system modules,
and the game's data directories. The preparer accepts plaintext PS4 SELF files;
it does not decrypt encrypted packages or download a game or firmware.

**Linux x86-64 — currently tested on Fedora 44.**

The runtime requires Python3, the Vulkan loader, a working Vulkan driver,
SDL3, FFmpeg shared libraries, fmt, xxhash, Zydis, and X11 libraries. The current
development GPU library requires **glibc 2.43 or later**, plus compatible versions
of the other shared libraries. There is no Fedora-only restriction: another
distribution with compatible dependencies may run the same binaries, but has
not yet been verified. Missing libraries can be installed through that distro's
package manager where compatible versions are available. KDE itself does not
determine library compatibility. System libraries and drivers are not bundled.
If compatible versions are unavailable, the included source can be built against
that distribution's dependencies. A future AppImage needs an older build baseline and its own
dependency/license review; the inherited AppImage builder is not adapted.

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

## Maintainer instructions

Create a focused source checkpoint after verification, then preview the exact
export. The exporter refuses to build an archive from a dirty working tree.

```bash
python3 tools/package_valkyria.py --preview > out/source-preview.json
python3 tools/package_valkyria.py
PS4RT_LTO=OFF bash build.sh --test
python3 tools/package_valkyria.py --runtime --preview > out/runtime-preview.json
python3 tools/package_valkyria.py --runtime
```

Archives and SHA256 files appear in `out/community/`. The source export contains
tracked files under an explicit set of source directories, including vendored
dependencies, their licenses, build scripts, tests and engineering changes.
The runtime export contains that source under `source/`, two explicitly named
host binaries, four preparation helpers, the new launcher, optional asset fetch
scripts, the 189 bundled FSR 4 v07 assets and notice, this document and GPL text. `MANIFEST.json` records the revision and
SHA256 of each payload file. Build the binaries from that same checkpoint.
The package is a local candidate; creating it does not publish or upload it.

The exporter does **not** copy directories from `out/`, originals, saves,
screenshots, shader/pipeline caches, prepared executables, PS4 modules, FSR 4.1.1
DLLs or models. Inherited launcher artwork/font with unreviewed provenance
is excluded from the source export. No new game artwork is needed for the port UI.
Do not share an arbitrary ZIP of this workspace, its Git metadata/history, or
the development checkpoints folder instead of the reviewed export.

Retain the source and every included third-party notice with a binary release.
The port's source retains its GPL2-or-later notices. The Fedora FFmpeg build
used here reports GPLv3-or-later; the candidate also includes GPLv3 text at
`licenses/GPL-3.0.txt` (source copy: `docs/licenses/GPL-3.0.txt`) for distribution
under the compatible later GPL version. Individual components keep their own
notices, including MIT and other licenses on:
FSR-Vulkan/AMD FidelityFX, ImGui, LibAtrac9, Sirit, half, xbyak, miniz,
magic_enum, AMD register headers, and DejaVu fonts. Their notices and corresponding
vendored source are in the source export. Dependencies supplied by Fedora remain
separate system packages; bundling their binaries would require collecting their
licenses and matching source as applicable. In particular, FFmpeg's obligations
depend on its build configuration, so do not copy a system library tree blindly.

Primary license references: [GNU GPLv2](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
and [FFmpeg's distribution guidance](https://ffmpeg.org/legal.html).
A game-content-free export is an engineering boundary, not a guarantee about every
jurisdiction's copyright, circumvention or trademark rules. This document does
not grant permission to distribute Sega/Sony game content or use their branding.

Before a public release: test the extracted candidate on the declared target,
verify the checksums/file list, provide its matching source archive and attribution,
and describe the actual tested gameplay coverage and known limitations. Publication
and uploading remain a separate owner decision.
