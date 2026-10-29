# Valkyria Revolution Linux release builder

This reproduces the Ubuntu 22.04 / glibc 2.35 / GCC 14 / FFmpeg 8 developer build environment first validated on Star Ocean and then gameplay-tested with Valkyria. Players **do not** need Podman or development libraries.

From this source repository:

```bash
# Only if you do not already have localhost/ps4rt-builder:linux:
bash packaging/linux/build-images.sh

# Clean committed-source build, isolated from your game and saves:
bash packaging/linux/build-runtime.sh
```

The second command prints a `BUNDLE=/absolute/path` at the end. Once its executable has passed gameplay testing, package it:

```bash
BUNDLE="/path/printed/by/build-runtime.sh"
python3 tools/package_valkyria.py --runtime --bundle-from "$BUNDLE" --preview
python3 tools/package_valkyria.py
python3 tools/package_valkyria.py --runtime --bundle-from "$BUNDLE"
(cd out/community && sha256sum -c *.sha256)
```

The source and runtime archives keep the existing
`valkyria-revolution-linux-x86_64-<commit>.tar.gz` and
`valkyria-revolution-source-<commit>.tar.gz` names, each with a
`.sha256` companion. Bundled runtime includes `bin/ps4rt`, `bin/gpu/libps4rtgpu.so`, `bin/ps4rt-gpu-capabilities`, and 12 audited shared libraries. Library lookup uses relative `$ORIGIN` paths. Keep host glibc 2.35+, Python 3, and a working Vulkan driver.

`Play-Game.sh` is the canonical player launcher, with `Play-Valkyria.sh` retained as a compatibility alias. First launch: `bash Play-Game.sh --game "/absolute/path/to/CUSA06810"`. After a valid first launch, `bash Play-Game.sh` uses the remembered folder. An explicit `--game` always overrides and updates that path. No Tkinter or graphical folder picker is used. Existing `--data-dir`, save files, restart logic and preparation behavior remain unchanged. The old Windows build entry points have been removed; Linux is the supported target.

The final builder image includes **PNG-enabled FFmpeg**, needed for in-game
thumbnail decoding. Earlier image stages without that decoder must not be
used for release bundles.

The image recipes are not bit-for-bit reproducible because some toolchain packages are not pinned to exact build artifacts. SteamOS remains unverified; gameplay testing was on Fedora. The [v1.1 release](https://github.com/HekamiAeon/valkyria-revolution-ps4rt/releases/latest) is already public. Maintainers should complete
any outstanding bundled-library notice/matching-source and overlay-media rights
review documented in `packaging/linux/THIRD_PARTY_REVIEW.md`. Do not put game files, prepared executables, user saves, shaders, captures, or firmware into published archives.

**Release workflow:** validate the built bundle, build the exact committed-source archive, test the freshly extracted archive, then publish separately. v1.0 is retained as a superseded historical release; v1.1 is the recommended download.

## Tracked overlay artwork and audio

The reviewed player media files are tracked in the repository at these paths:

```text
gpu/assets/valkyria_overlay.png
gpu/assets/sounds/navigate.wav
gpu/assets/sounds/confirm.wav
gpu/assets/sounds/cancel.wav
```

`open.wav` is optional. The Star Ocean audio player uses a synthesized fallback
when any WAV is missing. The game opens the menu with the confirm sound, as in
Star Ocean. The portrait is drawn **inside** the translucent wine-red menu
panel, center-cropped to preserve its aspect ratio. Menu dimensions match Star
Ocean's compact 1080x650 design at 1080p. The window-mode dropdown already
existed and is now available in Valkyria's compact graphics page.

The recorded files must be ordinary RIFF WAV files under 2 MiB each.
The portrait should be PNG, maximum 4096x4096. The build script copies
only these named files from the committed source snapshot into the isolated
build bundle; the runtime exporter includes only media from that tested bundle.
The public source archive also includes the tracked media, so a clean clone can
reproduce the player's overlay. The executable still works without them.

**Before committing or publishing artwork or audio, verify redistribution rights,
attributions and any required permission.** The packaging step verifies format/size,
not copyright or ownership.

The **player archive excludes the entire `docs/` tree**, with only required
license/credit files copied to top-level `licenses/` and `CREDITS.md`. The
source archive includes a fixed allowlist of public guides, the reviewed GPL notice, and the
single README screenshot at `docs/images/readme/gameplay.png`; other files in
`docs/` are not exported.
