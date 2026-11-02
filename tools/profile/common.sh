# Shared helpers for the profiling scripts; sourced, not executed.
# Output goes to build/profile-reports/<tool>-<exe>-<timestamp>.*
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
REPORTS="${REPO}/build/profile-reports"
mkdir -p "${REPORTS}"
stamp() { date +%Y%m%d-%H%M%S; }
need() {
  command -v "$1" >/dev/null 2>&1 || { echo "error: $1 not installed" >&2; exit 1; }
}
usage_exe() {
  if [[ $# -lt 1 ]]; then
    echo "usage: $(basename "$0") <executable> [args...]" >&2
    echo "  build first with: cmake --preset profile && cmake --build --preset profile" >&2
    exit 2
  fi
}
