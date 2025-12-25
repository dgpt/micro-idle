#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "--help" ]]; then
  echo "Usage: bin/build.sh [build_dir]"
  exit 0
fi

build_dir=${1:-build}

mkdir -p "$build_dir"
cmake -S . -B "$build_dir"
cmake --build "$build_dir"
