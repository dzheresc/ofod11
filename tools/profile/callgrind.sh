#!/usr/bin/env bash
# Instruction-level profile with Callgrind; open the .out file in KCachegrind.
#   tools/profile/callgrind.sh build/profile/cpp/bench/ofod_bench \
#       --benchmark_filter=BmCrrAmerican/1024
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need valgrind
out="${REPORTS}/callgrind-$(basename "$1")-$(stamp).out"
valgrind --tool=callgrind --callgrind-out-file="${out}" --dump-instr=yes "$@"
callgrind_annotate --auto=no "${out}" | head -40
echo "report: ${out}"
