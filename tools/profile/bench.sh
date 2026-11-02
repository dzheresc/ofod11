#!/usr/bin/env bash
# Builds the profile preset and runs the benchmark suite, saving JSON for
# comparison against docs/perf/baseline.json:
#   tools/profile/bench.sh [extra benchmark flags]
source "$(dirname "$0")/common.sh"
cd "${REPO}"
cmake --preset profile >/dev/null
cmake --build --preset profile --target ofod_bench
out="${REPORTS}/bench-$(stamp).json"
./build/profile/cpp/bench/ofod_bench --benchmark_out="${out}" \
  --benchmark_out_format=json "$@"
echo "results: ${out}"
