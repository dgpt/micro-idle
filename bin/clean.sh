#!/usr/bin/env bash
set -euo pipefail

# Single build output: build (Windows cross-compile)
build_dir="build"

if [[ -d "$build_dir" ]]; then
  rm -rf "$build_dir"
  echo "Removed $build_dir"
else
  echo "No build dir to clean ($build_dir)"
fi
