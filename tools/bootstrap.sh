#!/usr/bin/env bash
# Build and test all three components inside the dev container.
# Usage: ./tools/bootstrap.sh [cmake-preset]   (default: gcc-debug)
set -euo pipefail

PRESET="${1:-gcc-debug}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

echo "==> [1/3] C++ library ($PRESET)"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET"
ctest --preset "$PRESET"

echo "==> [2/3] Pure-Python library"
uv venv --allow-existing --python python3 .venv
# shellcheck disable=SC1091
source .venv/bin/activate
uv pip install -e "python[dev]"
pytest -q python/tests

echo "==> [3/3] Bindings"
uv pip install "./bindings[dev]"
pytest -q bindings/tests

echo "==> All components built and tested."
