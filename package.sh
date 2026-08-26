#!/usr/bin/env bash
# Neo-Wolf Release Packaging Script (Linux)
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

VERSION="${1:-1.0.0}"
OUT_DIR="${2:-release}"

echo "Packaging Neo-Wolf v${VERSION} to ${OUT_DIR}..."
python3 tools/devkit/devkit.py package --out-dir "$OUT_DIR" --version "$VERSION"

echo "Release package created successfully in ${OUT_DIR}/"
