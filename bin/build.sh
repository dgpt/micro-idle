#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: bin/build.sh"
  echo "  Cross-compiles Windows binaries (game.exe, tests.exe) into build/ using x86_64-w64-mingw32-gcc"
}

if [[ "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

build_dir="build"
rm -rf "$build_dir"
mkdir -p "$build_dir"

cmake -S . -B "$build_dir" \
  -DCMAKE_SYSTEM_NAME=Windows \
  -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
  -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
  -DCMAKE_BUILD_TYPE=Release

cmake --build "$build_dir"
