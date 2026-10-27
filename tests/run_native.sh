#!/bin/sh
# Build and run the native GLIDER DSP tests with the host compiler.
set -e
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/steel-tests"; mkdir -p "$out"
cc="clang -std=c11 -O2 -Wall -Wextra -DFM1_HOST_TEST -Iapp -Isdk/include"
$cc tests/steel_dsp_test.c app/steel_fx.c app/steel_reverb.c app/steel_drums_dsp.c -o "$out/dsp" -lm
$cc tests/steel_fx_test.c -o "$out/fx" -lm
$cc tests/steel_tempo_reverb_test.c app/steel.c app/steel_dsp.c app/steel_fx.c app/steel_reverb.c \
    app/steel_drums_dsp.c -o "$out/tempo" -lm
$cc tests/steel_guard_test.c app/steel.c app/steel_dsp.c app/steel_fx.c app/steel_reverb.c \
    app/steel_drums_dsp.c -o "$out/guard" -lm
$cc tests/steel_drums_dsp_test.c app/steel_drums_dsp.c -o "$out/drums" -lm
status=0
for t in dsp fx tempo guard drums; do "$out/$t" || status=1; done
exit $status
