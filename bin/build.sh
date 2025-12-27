#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: bin/build.sh [--clean]"
  echo "  Cross-compiles Windows binaries (game.exe, tests.exe) into build/ using x86_64-w64-mingw32-gcc"
  echo "  --clean: Force full rebuild (default: incremental)"
}

if [[ "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

build_dir="build"

# Only clean if explicitly requested
if [[ "${1:-}" == "--clean" ]]; then
  echo "Clean build requested, removing $build_dir..."
  rm -rf "$build_dir"
fi

mkdir -p "$build_dir"

# Only reconfigure if build dir doesn't exist or --clean was used
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  cmake -S . -B "$build_dir" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE=Release
fi

# Incremental build (only recompiles changed files)
cmake --build "$build_dir" -j$(nproc)
