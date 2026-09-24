#!/usr/bin/env bash
set -euo pipefail
repo=$(cd "$(dirname "$0")/.." && pwd)
kit="${PIN_ROOT:-$repo/work/pin/pin-external-3.31-98869-gfa6f126a8-gcc-linux}"
build="${TRACER_BUILD_DIR:-$repo/work/tracer_v2_build}"
if [[ "$build" != /* ]]; then
  build="$repo/$build"
fi
mkdir -p "$build/tracer/pin" "$build/inc"
cp "$repo/patches/tracer/champsim_tracer.cpp" "$build/tracer/pin/champsim_tracer.cpp"
cp "$repo/third_party/ChampSim/tracer/pin/Makefile" "$build/tracer/pin/Makefile"
cp "$repo/third_party/ChampSim/inc/trace_instruction.h" "$build/inc/trace_instruction.h"
make -C "$build/tracer/pin" PIN_ROOT="$kit" -j4
