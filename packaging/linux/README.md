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

`Play-Valkyria.sh`, the CUSA06810 folder selector, and `--data-dir` behavior are unchanged. Windows retains its original CMake target.

The final builder image includes **PNG-enabled FFmpeg**, needed for in-game
thumbnail decoding. Earlier image stages without that decoder must not be
used for release bundles.

The image recipes are not bit-for-bit reproducible because some toolchain packages are not pinned to exact build artifacts. SteamOS remains unverified; gameplay testing was on Fedora. **Do not publish v1.1** until the bundled library notices and matching-source
review in `packaging/linux/THIRD_PARTY_REVIEW.md` is complete. Do not put game files, prepared executables, user saves, shaders, captures, or firmware into published archives.

**Release workflow:** validate the built bundle, build the exact committed-source archive, test the freshly extracted archive, then publish separately. The existing v1.0 release is untouched.
