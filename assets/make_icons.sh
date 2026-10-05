#!/usr/bin/env bash
# Пересобирает иконку из assets/ksnake.svg: PNG-набор + .ico для Windows.
# Вручную запускать только после правки ksnake.svg; в релизной сборке
# используются уже готовые файлы из assets/.
set -euo pipefail

DIR="$(cd "$(dirname "$0")" && pwd)"
SVG="$DIR/ksnake.svg"
SIZES=(16 24 32 48 64 128 256 512)

command -v rsvg-convert >/dev/null 2>&1 || {
    echo "нужен rsvg-convert (librsvg): sudo pacman -S librsvg" >&2
    exit 1
}
CONV="$(command -v magick || command -v convert || true)"
[[ -n "$CONV" ]] || {
    echo "нужен ImageMagick (magick или convert)" >&2
    exit 1
}

pngs=()
for size in "${SIZES[@]}"; do
    rsvg-convert -w "$size" -h "$size" "$SVG" -o "$DIR/ksnake-$size.png"
    pngs+=("$DIR/ksnake-$size.png")
done
cp -f "$DIR/ksnake-512.png" "$DIR/ksnake.png"

# В .ico Windows берёт максимум 256x256, поэтому 512-кадр в него не кладу.
ico_pngs=()
for size in "${SIZES[@]}"; do
    [[ "$size" == "512" ]] && continue
    ico_pngs+=("$DIR/ksnake-$size.png")
done
"$CONV" "${ico_pngs[@]}" "$DIR/ksnake.ico"

echo "готово: ${SIZES[*]}"
"$CONV" identify "$DIR/ksnake.ico" | sed 's/^/  /'