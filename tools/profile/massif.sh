#!/usr/bin/env bash
# Heap profile over time with Massif; view with ms_print or massif-visualizer.
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need valgrind
out="${REPORTS}/massif-$(basename "$1")-$(stamp).out"
valgrind --tool=massif --massif-out-file="${out}" "$@"
ms_print "${out}" | head -60
echo "report: ${out}"
