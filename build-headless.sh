#!/usr/bin/env bash
# CPU execution diagnostics without Vulkan, SDL3 or FFmpeg development packages.
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p out/headless
cc=${CC:-cc}
flags=(-std=c11 -O2 -g -Wall -Wextra -Werror -pthread -DVALKYRIA_HEADLESS -I. -Isrc)
runtime=()
for source in src/runtime*.c; do
    case "$source" in
        src/runtime_ajm.c|src/runtime_audio.c|src/runtime_mp3.c|src/runtime_pad.c|src/runtime_videodec.c) ;;
        *) runtime+=("$source") ;;
    esac
done
"$cc" "${flags[@]}" -mgeneral-regs-only -c src/isa_emulate.c -o out/headless/isa_emulate.o
"$cc" "${flags[@]}" -no-pie -rdynamic src/probe.c "${runtime[@]}" src/headless.c \
    src/cpu_check.c src/isa_rewrite.c out/headless/isa_emulate.o -lm -ldl -o out/valkyria-probe
echo "Built $PWD/out/valkyria-probe (headless; use --cpu-only)"
if [[ ${1:-} == --test ]]; then
    "$cc" "${flags[@]}" tests/test_runtime.c "${runtime[@]}" src/headless.c -lm -ldl -o out/headless/runtime-test
    out/headless/runtime-test
    VALKYRIA_PROBE=out/valkyria-probe python3 -m unittest discover -s tests -p test_probe.py
fi
