#!/usr/bin/env bash
# Native Valkyria startup without Ps4Rt preparation patches or launcher assumptions.
set -euo pipefail
cd -- "$(dirname -- "$0")"
if [[ ! -x out/ps4rt-probe ]]; then
    echo 'Build first: PS4RT_LTO=OFF bash build.sh --test' >&2
    exit 1
fi
valkyria_game=${VALKYRIA_GAME_DIR:-../original/CUSA06810}
valkyria_out="$PWD/out/valkyria"
mkdir -p "$valkyria_out/user/gpu" "$valkyria_out/frame-dump"
python3 scripts/prepare.py "$valkyria_game" --out "$valkyria_out"
python3 scripts/link_modules.py "$valkyria_game" --out "$valkyria_out"
python3 scripts/content_profile.py "$valkyria_game" --out "$valkyria_out"
export PS4RT_GPU_USER_DIR="$valkyria_out/user/gpu"
export PS4RT_CONFIG="$valkyria_out/valkyria.ini"
export PS4RT_CAPTURE_TRIGGER="$valkyria_out/next-render.request"
export PS4RT_CAPTURE_DIR="$valkyria_out/captures"
export PS4RT_FRAME_DUMP_TRIGGER="$valkyria_out/next-frame.request"
export PS4RT_DUMP_DIR="$valkyria_out/frame-dump"
export PS4RT_DUMP_TRIGGER="$valkyria_out/next-temporal.request"
export PS4RT_DUMP_FRAMES="${PS4RT_DUMP_FRAMES:-2}"
exec "$PWD/out/ps4rt-probe" "$valkyria_out/boot-linked.bin" \
    --app0 "$valkyria_game" --user "$valkyria_out/user" \
    --content-profile "$valkyria_out/content.bin" --timeout 300 "$@"
