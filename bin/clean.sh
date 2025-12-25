#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "--help" ]]; then
  echo "Usage: bin/clean.sh [build_dir] [--hard]"
  exit 0
fi

build_dir=${1:-build}
mode=${2:-}

if [[ ! -d "$build_dir" ]]; then
  mkdir -p "$build_dir"
  exit 0
fi

if [[ "$mode" == "--hard" ]]; then
  rm -rf "$build_dir"
  exit 0
fi

rm -f "$build_dir/CMakeCache.txt"
rm -rf "$build_dir/CMakeFiles"
rm -f "$build_dir/cmake_install.cmake" "$build_dir/Makefile"
echo "Cleaned CMake cache. Run bin/build.sh to reconfigure."
