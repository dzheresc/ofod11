#!/usr/bin/env bash
# Allocation hot spots with heaptrack; view with heaptrack_gui.
source "$(dirname "$0")/common.sh"
usage_exe "$@"
need heaptrack
heaptrack -o "${REPORTS}/heaptrack-$(basename "$1")-$(stamp)" "$@"
