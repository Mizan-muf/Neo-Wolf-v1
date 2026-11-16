#!/usr/bin/env bash
# Neo-Wolf release package for this OS (Roadmap Phase 9, docs/RELEASE.md):
#   macOS: release/neo-wolf-<version>-macos-universal.zip (NeoWolf.app, signed ad hoc)
#   Linux: release/neo-wolf-<version>-linux-x86_64.tar.gz (best effort)
# plus release/SHA256SUMS. The version comes from project(VERSION) in CMakeLists.txt.
#
# Usage: ./package.sh [DevKit package options]
#   ./package.sh                          # configure + build the package preset, install, archive
#   ./package.sh --out-dir dist --clean   # another folder, without older archives
#   ./package.sh --expect-version 1.0.0   # fail unless CMakeLists.txt says 1.0.0 (release tags)
# All options: python3 tools/devkit/devkit.py package --help. The DevKit's exit code is passed on
# (non-zero when the build fails or the package has no executable).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ $# -gt 0 && "$1" != -* ]]; then
    echo "error: package.sh takes no version argument any more: the version comes from" >&2
    echo "  project(VERSION) in CMakeLists.txt (docs/RELEASE.md). Options: --out-dir DIR, --expect-version X.Y.Z" >&2
    exit 2
fi

PYTHON="${PYTHON:-python3}"
if ! command -v "$PYTHON" >/dev/null 2>&1; then
    echo "error: '$PYTHON' not found. Install Python 3.8 or newer." >&2
    exit 9
fi

exec "$PYTHON" "$SCRIPT_DIR/tools/devkit/devkit.py" package "$@"
