#!/usr/bin/env bash
# Neo-Wolf build script for macOS and Linux.
#
# Usage: ./build.sh [Release|Debug]
#
# Configures with the CMake preset for this OS (macos / linux, or the -debug variant),
# builds, then runs every test with CTest. Any failure stops the script with a non-zero
# exit code. See docs/BUILD_MACOS.md and docs/TESTING.md.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

CONFIG="${1:-Release}"
case "$CONFIG" in
    Release|release) SUFFIX="" ; CONFIG="Release" ;;
    Debug|debug)     SUFFIX="-debug" ; CONFIG="Debug" ;;
    -h|--help)
        echo "Usage: $0 [Release|Debug]"
        exit 0
        ;;
    *)
        echo "error: unknown configuration '$CONFIG' (expected Release or Debug)" >&2
        exit 2
        ;;
esac

case "$(uname -s)" in
    Darwin) PRESET="macos${SUFFIX}" ;;
    Linux)  PRESET="linux${SUFFIX}" ;;
    *)
        echo "error: unsupported OS '$(uname -s)'. On Windows use build.cmd or build.ps1." >&2
        exit 2
        ;;
esac

for tool in cmake ctest ninja; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: '$tool' not found on PATH." >&2
        echo "  macOS: brew install cmake ninja    Linux: install cmake and ninja-build" >&2
        exit 1
    fi
done

echo "==> Neo-Wolf: preset '$PRESET' ($CONFIG)"
echo "==> Configure"
cmake --preset "$PRESET"
echo "==> Build"
cmake --build --preset "$PRESET"
echo "==> Test"
ctest --preset "$PRESET"

echo "==> Build and tests succeeded. Run the game from the repository root:"
echo "    ./build/$PRESET/neo_wolf"
