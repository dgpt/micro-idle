#!/usr/bin/env bash
set -euo pipefail

usage() {
  echo "Usage: bin/build.sh [--clean] [--release]"
  echo "  Cross-compiles Windows .exe files (game.exe, tests.exe) into build/"
  echo "  --clean: Force full rebuild (default: incremental)"
  echo "  --release: Build in Release mode (default: RelWithDebInfo)"
}

if [[ "${1:-}" == "--help" ]]; then
  usage
  exit 0
fi

build_dir="build"
build_type="RelWithDebInfo"

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --clean)
      echo "Clean build requested, removing $build_dir..."
      rm -rf "$build_dir"
      shift
      ;;
    --release)
      build_type="Release"
      shift
      ;;
    *)
      echo "Unknown option: $1"
      usage
      exit 1
      ;;
  esac
done

mkdir -p "$build_dir"

# Reconfigure if needed
if [[ ! -f "$build_dir/CMakeCache.txt" ]]; then
  echo "Configuring for Windows cross-compilation (build type: $build_type)..."
  cmake -S . -B "$build_dir" \
    -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
    -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
    -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
    -DCMAKE_BUILD_TYPE="$build_type"
fi

# Incremental build (only recompiles changed files)
echo "Building Windows executables..."
cmake --build "$build_dir" -j$(nproc)

echo ""
echo "Build complete!"
echo "  Game: $build_dir/game.exe"
echo "  Tests: $build_dir/tests.exe"
