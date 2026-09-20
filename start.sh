#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"

if [[ ! -x "$BUILD_DIR/snake" ]]; then
    echo "Binary not found at $BUILD_DIR/snake. Building first..."
    "$SCRIPT_DIR/build.sh"
fi

cd "$SCRIPT_DIR"
exec "$BUILD_DIR/snake" --debug /dev/stdout