#!/usr/bin/env bash
# Memory errors and leaks with Memcheck; non-zero exit on any error.
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need valgrind
valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
  --error-exitcode=1 "$@"
