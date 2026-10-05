#!/usr/bin/env bash
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
    echo "Usage: $0 [VERSION]"
    echo
    echo "  VERSION  release tag, default: contents of the VERSION file"
    echo
    echo "Windows release is built as a single self-contained exe (DLLs/font embedded)."
    exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$SCRIPT_DIR"

VERSION="${1:-$(tr -d '[:space:]' < "$ROOT/VERSION")}"

say() { printf '\033[1;32m[release]\033[0m %s\n' "$*"; }
fail() { printf '\033[1;31m[release] ERROR:\033[0m %s\n' "$*" >&2; exit 1; }

# Тег релиза подставляется в пути (final_build/$VERSION, KSnake-$VERSION-*.AppImage)
# и в текст .desktop, поэтому проверяем его ДО первого rm/mkdir: строка вида
# "../../etc" иначе уводила бы rm -rf за пределы репозитория.
[[ -n "$VERSION" ]] || fail "VERSION is empty"
[[ "$VERSION" =~ ^[0-9]+(\.[0-9]+)*$ ]] \
    || fail "VERSION '$VERSION' is not a dotted number (expected e.g. 1.1 or 1.2.0)"

OUT_DIR="$ROOT/final_build/$VERSION"
CACHE_DIR="$ROOT/.release-cache"
LINUX_BUILD_DIR="$ROOT/.build-release"
WIN_BUILD_DIR="$ROOT/.build-win"

SDL2_VER="2.32.10"
SDL2_TTF_VER="2.24.0"

# rm -rf только по путям, которые мы сами вычислили внутри репозитория.
# Пустая строка или "/" здесь означали бы удаление не того каталога.
safe_rm() {
    local target="$1" resolved root_resolved
    [[ -n "$target" ]] || fail "safe_rm: empty path"
    resolved="$(realpath -m "$target")"
    root_resolved="$(realpath -m "$ROOT")"
    case "$resolved" in
        "$root_resolved"|"$root_resolved"/*) ;;
        *) fail "safe_rm: $resolved is outside $root_resolved" ;;
    esac
    rm -rf -- "$target"
}

safe_rm "$OUT_DIR"
mkdir -p "$OUT_DIR" "$CACHE_DIR"

TOOLS="curl unzip x86_64-w64-mingw32-g++ x86_64-w64-mingw32-windres"
for tool in $TOOLS; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing required tool: $tool"
done

# Иконка коммитится в assets/ наряду с исходным SVG: пересобрать её можно
# через assets/make_icons.sh, но для релиза готовых файлов достаточно.
ICON_SRC="$ROOT/assets/ksnake.png"
ICON_ICO="$ROOT/assets/ksnake.ico"
for icon in "$ICON_SRC" "$ICON_ICO"; do
    [[ -f "$icon" ]] || fail "missing icon: $icon (run assets/make_icons.sh)"
done

download() {
    local url="$1" dest="$2"
    if [[ ! -s "$dest" ]]; then
        say "Downloading $(basename "$dest")..."
        # -f обязателен: без него curl кладёт в dest тело страницы с HTTP-ошибкой,
        # и битый zip/ico молча уходит дальше. -L нужен для редиректов GitHub,
        # --retry переживает кратковременные сбои сети.
        curl -fsSL --retry 3 --retry-delay 2 -o "$dest" "$url" \
            || fail "failed to download $url"
        [[ -s "$dest" ]] || fail "downloaded file is empty: $dest"
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
safe_rm "$APPDIR"
mkdir -p "$APPDIR/usr/bin/assets"
mkdir -p "$APPDIR/usr/share/applications"

cp "$LINUX_BUILD_DIR/snake" "$APPDIR/usr/bin/snake"
cp "$ROOT/assets/font.ttf" "$APPDIR/usr/bin/assets/font.ttf"
cp "$ROOT/VERSION" "$APPDIR/usr/bin/VERSION"

cat > "$APPDIR/usr/bin/KSnake.sh" <<'EOF'
#!/bin/sh
# AppRun указывает сюда симлинком. Рядом с exe лежат assets/font.ttf и VERSION,
# поэтому запускаться нужно из своего каталога: иначе шрифт не находится.
SELF="$(readlink -f "$0")"
DIR="$(dirname "$SELF")"
cd "$DIR" || exit 1
exec ./snake "$@"
EOF
chmod +x "$APPDIR/usr/bin/KSnake.sh"

# Exec обязан быть абсолютным путём либо именем, которое ищется в $PATH.
# Голое "KSnake.sh" не то и не другое: в $PATH его нет, и запись в меню
# не запускает игру. Внутри AppImage корнем является AppRun, поэтому указываем
# его — и создаём сами: linuxdeploy ищет файл из Exec в appdir и без симлинка
# падает с "could not find suitable executable for Exec entry".
ln -sf usr/bin/KSnake.sh "$APPDIR/AppRun"

cat > "$APPDIR/usr/share/applications/KSnake.desktop" <<EOF
[Desktop Entry]
Name=KSnake
Comment=Classic snake game written in C++ with SDL2
Exec=AppRun
Icon=ksnake
Type=Application
Categories=Game;
EOF

for icon_size in 16 24 32 48 64 128 256; do
    icon_dir="$APPDIR/usr/share/icons/hicolor/${icon_size}x${icon_size}/apps"
    mkdir -p "$icon_dir"
    cp "$ROOT/assets/ksnake-$icon_size.png" "$icon_dir/ksnake.png"
done
ICON_PNG="$APPDIR/usr/share/icons/hicolor/256x256/apps/ksnake.png"
# linuxdeploy создаёт .DirIcon сам, но без готового файла берёт 64x64 —
# задаём 256x256 явно, иначе иконка мылится в превью файловых менеджеров.
cp "$ROOT/assets/ksnake-256.png" "$APPDIR/.DirIcon"

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

safe_rm "$WIN_BUILD_DIR"
cmake -S "$ROOT" -B "$WIN_BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$CACHE_DIR/toolchain-mingw.cmake" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$SDL_SRC;$TTF_SRC" \
    -DCMAKE_RUNTIME_OUTPUT_DIRECTORY="$WIN_BUILD_DIR" >/dev/null
cmake --build "$WIN_BUILD_DIR" -j"$(nproc)" >/dev/null
[[ -x "$WIN_BUILD_DIR/snake.exe" ]] || fail "windows build did not produce snake.exe"

WIN_DIR="$CACHE_DIR/KSnake-windows"
safe_rm "$WIN_DIR"
mkdir -p "$WIN_DIR/assets"
cp "$WIN_BUILD_DIR/snake.exe" "$WIN_DIR/KSnake.exe"
cp "$SDL_SRC/bin/SDL2.dll" "$WIN_DIR/"
cp "$TTF_SRC/bin/SDL2_ttf.dll" "$WIN_DIR/"
cp "$ROOT/assets/font.ttf" "$WIN_DIR/assets/font.ttf"
cp "$ROOT/VERSION" "$WIN_DIR/VERSION"
cp "$ICON_ICO" "$WIN_DIR/ksnake.ico"

WIN_EXE="KSnake-$VERSION-windows-x86_64.exe"
say "Building single-file Windows EXE..."
cat > "$WIN_DIR/KSnake.rc" <<'EOF'
1 ICON "ksnake.ico"
101 RCDATA "KSnake.exe"
102 RCDATA "SDL2.dll"
103 RCDATA "SDL2_ttf.dll"
104 RCDATA "assets/font.ttf"
105 RCDATA "VERSION"
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