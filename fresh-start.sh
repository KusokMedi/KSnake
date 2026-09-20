#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$SCRIPT_DIR/build}"

echo "Removing stale build dir: $BUILD_DIR"
rm -rf "$BUILD_DIR"

"$SCRIPT_DIR/build.sh"

cd "$SCRIPT_DIR"
exec "$BUILD_DIR/snake" --debug /dev/stdout