#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    echo "Usage: $0 [VERSION]"
    echo
    echo "  VERSION  release tag, default: temp"
    echo
    echo "Windows release is built as a single self-contained exe (DLLs/font embedded)."
    exit 0
fi

VERSION="${1:-temp}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR"
OUT_DIR="$ROOT/final_build/$VERSION"
CACHE_DIR="$ROOT/.release-cache"
LINUX_BUILD_DIR="$ROOT/.build-release"
WIN_BUILD_DIR="$ROOT/.build-win"

SDL2_VER="2.32.10"
SDL2_TTF_VER="2.24.0"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR" "$CACHE_DIR"

say() { printf '\033[1;32m[release]\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31m[release] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

TOOLS="curl unzip convert x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres"
for tool in $TOOLS; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing required tool: $tool"
done

download() {
    local url="$1" dest="$2"
    if [[ ! -f "$dest" ]]; then
        say "Downloading $(basename "$dest")..."
        curl -sL -o "$dest" "$url" || fail "failed to download $url"
    fi
}

say "Version: $VERSION"
say "Output:  $OUT_DIR"

# ---------------------------------------------------------------- Linux build
say "Building Linux (Release)..."
cmake -S "$ROOT" -B "$LINUX_BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$LINUX_BUILD_DIR" >/dev/null
cmake --build "$LINUX_BUILD_DIR" -j"$(nproc)" >/dev/null
[[ -x "$LINUX_BUILD_DIR/snake" ]] || fail "linux build did not produce snake binary"

# ---------------------------------------------------------------- AppImage
say "Preparing AppImage..."
LD_TOOL="$CACHE_DIR/linuxdeploy-x86_64.AppImage"
download "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LD_TOOL"
chmod +x "$LD_TOOL"

APPIMAGE_NAME="KSnake-$VERSION-x86_64.AppImage"
APPDIR="$CACHE_DIR/KSnake-$VERSION-x86_64.AppDir"
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin/assets"
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$LINUX_BUILD_DIR/snake" "$APPDIR/usr/bin/snake"
cp "$ROOT/assets/font.ttf" "$APPDIR/usr/bin/assets/font.ttf"

cat > "$APPDIR/usr/bin/KSnake.sh" <<'EOF'
#!/bin/sh
SELF="$(readlink -f "$0")"
DIR="$(dirname "$SELF")"
cd "$DIR" || exit 1
exec ./snake "$@"
EOF
chmod +x "$APPDIR/usr/bin/KSnake.sh"

cat > "$APPDIR/usr/share/applications/KSnake.desktop" <<EOF
[Desktop Entry]
Name=KSnake
Comment=Classic snake game written in C++ with SDL2
Exec=KSnake.sh
Icon=ksnake
Type=Application
Categories=Game;
EOF

ICON_PNG="$APPDIR/usr/share/icons/hicolor/256x256/apps/ksnake.png"
convert -size 256x256 xc:'#141414' \
    -fill '#2ECC71' -draw "rectangle 112,32 143,63" \
    -fill '#111111' -draw "rectangle 121,40 127,46" -draw "rectangle 136,40 142,46" \
    -fill '#27AE60' -draw "rectangle 112,64 143,95" \
    -fill '#229954' -draw "rectangle 112,96 143,127" \
    "$ICON_PNG"

say "Packaging AppImage..."
(
    cd "$ROOT"
    APPIMAGE_EXTRACT_AND_RUN=1 "$LD_TOOL" \
        --appdir "$APPDIR" \
        --executable "$APPDIR/usr/bin/snake" \
        --desktop-file "$APPDIR/usr/share/applications/KSnake.desktop" \
        --icon-file "$ICON_PNG" \
        --output appimage
) || fail "AppImage packaging failed"
mv -f "$ROOT/KSnake-x86_64.AppImage" "$OUT_DIR/$APPIMAGE_NAME"

# ---------------------------------------------------------------- Windows exe
say "Building Windows EXE (cross)..."
SDL_SRC="$CACHE_DIR/sdl/SDL2-$SDL2_VER/x86_64-w64-mingw32"
TTF_SRC="$CACHE_DIR/sdl/SDL2_ttf-$SDL2_TTF_VER/x86_64-w64-mingw32"
if [[ ! -d "$SDL_SRC" || ! -d "$TTF_SRC" ]]; then
    mkdir -p "$CACHE_DIR/sdl"
    download "https://github.com/libsdl-org/SDL/releases/download/release-$SDL2_VER/SDL2-devel-$SDL2_VER-mingw.zip" "$CACHE_DIR/sdl/SDL2.devel.zip"
    download "https://github.com/libsdl-org/SDL_ttf/releases/download/release-$SDL2_TTF_VER/SDL2_ttf-devel-$SDL2_TTF_VER-mingw.zip" "$CACHE_DIR/sdl/SDL2_ttf.devel.zip"
    unzip -q -o "$CACHE_DIR/sdl/SDL2.devel.zip" -d "$CACHE_DIR/sdl"
    unzip -q -o "$CACHE_DIR/sdl/SDL2_ttf.devel.zip" -d "$CACHE_DIR/sdl"
fi
[[ -f "$SDL_SRC/include/SDL2/SDL.h" ]] || fail "SDL2 mingw headers not found"
[[ -f "$TTF_SRC/include/SDL2/SDL_ttf.h" ]] || fail "SDL2_ttf mingw headers not found"

cat > "$CACHE_DIR/toolchain-mingw.cmake" <<'EOF'
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
EOF

rm -rf "$WIN_BUILD_DIR"
cmake -S "$ROOT" -B "$WIN_BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$CACHE_DIR/toolchain-mingw.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$SDL_SRC;$TTF_SRC" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$WIN_BUILD_DIR" >/dev/null
cmake --build "$WIN_BUILD_DIR" -j"$(nproc)" >/dev/null
[[ -x "$WIN_BUILD_DIR/snake.exe" ]] || fail "windows build did not produce snake.exe"

WIN_DIR="$CACHE_DIR/KSnake-windows"
rm -rf "$WIN_DIR"
mkdir -p "$WIN_DIR/assets"
cp "$WIN_BUILD_DIR/snake.exe" "$WIN_DIR/KSnake.exe"
cp "$SDL_SRC/bin/SDL2.dll" "$WIN_DIR/"
cp "$TTF_SRC/bin/SDL2_ttf.dll" "$WIN_DIR/"
cp "$ROOT/assets/font.ttf" "$WIN_DIR/assets/font.ttf"

WIN_EXE="KSnake-$VERSION-windows-x86_64.exe"
say "Building single-file Windows EXE..."
cat > "$WIN_DIR/KSnake.rc" <<'EOF'
101 RCDATA "KSnake.exe"
102 RCDATA "SDL2.dll"
103 RCDATA "SDL2_ttf.dll"
104 RCDATA "assets/font.ttf"
EOF
x86_64-w64-mingw32-windres -O coff "$WIN_DIR/KSnake.rc" -o "$WIN_DIR/KSnake.res"
x86_64-w64-mingw32-g++ -O2 -s -municode -mwindows -static \
    "$ROOT/third_party/win_singlefile/singlefile.cpp" \
    "$WIN_DIR/KSnake.res" \
    -o "$WIN_DIR/KSnake-single.exe"
mv -f "$WIN_DIR/KSnake-single.exe" "$OUT_DIR/$WIN_EXE"

# ---------------------------------------------------------------- summary
say "Done. Artifacts:"
ls -lh "$OUT_DIR"/*.AppImage "$OUT_DIR"/*.exe 2>/dev/null || ls -lh "$OUT_DIR"