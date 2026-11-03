#!/usr/bin/env bash
# Sampling profile with perf; writes a flame graph if FlameGraph's scripts
# (stackcollapse-perf.pl, flamegraph.pl) are on PATH. In Docker Desktop,
# run the container with --cap-add SYS_ADMIN (or --privileged) for perf.
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need perf
base="${REPORTS}/perf-$(basename "$1")-$(stamp)"
perf record -F 999 -g -o "${base}.data" -- "$@"
perf report -i "${base}.data" --stdio | sed -n 1,60p
if command -v stackcollapse-perf.pl >/dev/null && command -v flamegraph.pl >/dev/null; then
  perf script -i "${base}.data" | stackcollapse-perf.pl | flamegraph.pl > "${base}.svg"
  echo "flame graph: ${base}.svg"
fi
echo "data: ${base}.data"
