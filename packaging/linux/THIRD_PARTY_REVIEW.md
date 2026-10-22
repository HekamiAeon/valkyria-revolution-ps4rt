# Third-party Linux runtime redistribution review

Bundled libraries include FFmpeg 8.0.3 (five libraries), SDL3, fmt, Zydis (and Zycore), xxHash, zlib, libstdc++, and libgcc_s.

Before public release, include required copyright and license notices, exact matching sources or valid source offers as applicable, and the FFmpeg configure flags showing PNG/zlib support. See https://ffmpeg.org/legal.html .

Keep PS4RT upstream GPL and vendored library credits. Exclude proprietary Valkyria game data, PS4 modules and firmware, saves, and captures. This document is for maintainers, not a player install requirement.
