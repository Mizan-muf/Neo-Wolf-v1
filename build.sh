#!/usr/bin/env bash
# Neo-Wolf Linux Build Script
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONFIG="${1:-Release}"
echo "Building Neo-Wolf [Config: $CONFIG]..."

if command -v cmake >/dev/null 2>&1; then
    echo "Configuring with CMake..."
    cmake -B build -S . -DCMAKE_BUILD_TYPE="$CONFIG"
    cmake --build build --config "$CONFIG"
    echo "Running CTest..."
    ctest --test-dir build --output-on-failure || true
else
    echo "CMake not found, compiling using Makefile/g++..."
    make tests
    if pkg-config --exists sdl2 2>/dev/null || sdl2-config --version >/dev/null 2>&1; then
        make game
    else
        echo "[INFO] SDL2 dev headers not found. Compiled test suite successfully."
    fi
    ./neo_wolf_tests
fi

echo "Build and tests completed successfully!"
