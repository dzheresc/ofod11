#!/usr/bin/env bash
# Cache simulation (I1/D1/LL misses) with Cachegrind.
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need valgrind
out="${REPORTS}/cachegrind-$(basename "$1")-$(stamp).out"
valgrind --tool=cachegrind --cache-sim=yes --cachegrind-out-file="${out}" "$@"
cg_annotate "${out}" | head -40
echo "report: ${out}"
