#!/usr/bin/env bash
# Data races and lock misuse with Helgrind (use on the threaded MC runner):
#   tools/profile/helgrind.sh build/profile/cpp/bench/ofod_bench \
#       --benchmark_filter=BmPathSimulator/4 --benchmark_min_time=0.01s
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need valgrind
valgrind --tool=helgrind --error-exitcode=1 "$@"
