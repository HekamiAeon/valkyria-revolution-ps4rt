# Third-party library redistribution checklist

**Not complete: do not publicly publish the Valkyria v1.1 binary release until resolved.**

The source archive contains the port's corresponding source, but the
twelve bundled shared libraries have separate licenses and may have
matching-source/notice obligations. This checklist is **not** a substitute
for including the required notices and sources in the release.

Application libraries: FFmpeg 8.0.3 (libavcodec, libavformat, libavutil,
libswscale and libswresample), SDL3, fmt11, Zydis5 (including Zycore),
xxhash, zlib, libstdc++ and libgcc_s. For each, record the exact
version/provenance, license and copyright notices, corresponding source
or permissible offer where applicable, and package the required files.

For FFmpeg, confirm actual compilation flags and that its license
configuration is LGPL-compatible: no `--enable-gpl` or `--enable-nonfree`
unless deliberately accounted for. Supply **exact matching FFmpeg source**,
configuration flags and patches, notices, and source access beside the
binary. PNG/zlib is enabled in the validated developer image.
See https://ffmpeg.org/legal.html .

Retain original PS4RT GPL-2.0-or-later notices and vendored dependency
notices, including FSR assets. Check the GCC Runtime Library Exception for
libstdc++ and libgcc_s. Review future overlay artwork, sound effects,
screenshots and in-game art separately before public redistribution.

Do not distribute proprietary CUSA06810 game files, PS4 modules/firmware,
game-derived executable code, user saves or captures. This checklist is
not legal advice.
