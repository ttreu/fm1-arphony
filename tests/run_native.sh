#!/bin/sh
# Build and run the native ARPHONY tests with the host compiler.
set -e
cd "$(dirname "$0")/.."
out="${TMPDIR:-/tmp}/arp-tests"; mkdir -p "$out"
cc="clang -std=c11 -O2 -Wall -Wextra -DFM1_HOST_TEST -Iapp -Iapp/synth -Isdk/include"
$cc tests/steel_fx_test.c -o "$out/fx" -lm
$cc tests/arp_copy_test.c app/arp_dsp.c app/steel_fx.c app/steel_reverb.c -o "$out/copy" -lm
# The instrument itself must build warning-free for the host too.
$cc -Werror -c app/arp.c -o "$out/arp.o"
$cc -Werror -c app/arp_dsp.c -o "$out/arp_dsp.o"
"$out/fx"
"$out/copy"
