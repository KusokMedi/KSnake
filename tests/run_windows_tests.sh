#!/usr/bin/env bash
# Windows-проверки KSnake под Wine: ветки util/storage, которые на Linux даже не
# компилируются — UTF-8-пути, атомарная запись рекорда, кавычение аргументов
# (quote_win_arg) и коды возврата дочерних процессов.
#
# Что нужно:
#   * x86_64-w64-mingw32-gcc/g++ — тем же тулчейном собирается релизный exe;
#   * wine — запускает собранный тест;
#   * MinGW-сборка SDL2 (include/SDL2 + lib/libSDL2.dll.a + bin/SDL2.dll).
#     build_release.sh кладёт её в .release-cache/sdl/; другой путь можно задать
#     через KS_SDL_MINGW_DIR=<.../SDL2-<версия>/x86_64-w64-mingw32>.
#
# Окон тест не открывает: SDL_Init не вызывается (SDL нужен только ради
# SDL_GetPrefPath внутри storage), поэтому дисплей и X-сервер не требуются.
#
# Запуск: ./tests/run_windows_tests.sh
# Коды возврата: 0 — тест прошёл, 1 — упал, 2 — нет инструментов или зависимостей.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2
OUT="$ROOT/tests/build/win"
LOG="$OUT/run.log"

for tool in x86_64-w64-mingw32-gcc x86_64-w64-mingw32-g++ wine; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "SKIP: нет $tool — Windows-тесты не запускались"
        exit 2
    fi
done

SDL_DIR="${KS_SDL_MINGW_DIR:-}"
if [ -z "$SDL_DIR" ]; then
    for d in "$ROOT"/.release-cache/sdl/SDL2-*/x86_64-w64-mingw32; do
        if [ -f "$d/include/SDL2/SDL.h" ] && [ -f "$d/lib/libSDL2.dll.a" ]; then
            SDL_DIR="$d"
            break
        fi
    done
fi
if [ ! -f "$SDL_DIR/include/SDL2/SDL.h" ] || [ ! -f "$SDL_DIR/lib/libSDL2.dll.a" ]; then
    echo "SKIP: не найдена MinGW-сборка SDL2 (нужны include/SDL2/SDL.h и lib/libSDL2.dll.a)."
    echo "      Получи её через build_release.sh или задай KS_SDL_MINGW_DIR."
    exit 2
fi

rm -rf "$OUT"; mkdir -p "$OUT"

echo "### сборка (SDL2: $SDL_DIR)"
if ! x86_64-w64-mingw32-gcc -O1 -Wall -Wextra -static -municode \
        tests/win_argdump.c -o "$OUT/win_argdump.exe" >"$OUT/build.log" 2>&1; then
    echo "  СБОРКА win_argdump.exe НЕ УДАЛАСЬ (см. $OUT/build.log)"
    exit 1
fi
if ! x86_64-w64-mingw32-g++ -std=c++17 -O1 -Wall -Wextra -static \
        -I src -I third_party -I "$SDL_DIR/include/SDL2" \
        tests/unit_win.cpp src/util.cpp src/storage.cpp "$SDL_DIR/lib/libSDL2.dll.a" \
        -o "$OUT/unit_win.exe" >>"$OUT/build.log" 2>&1; then
    echo "  СБОРКА unit_win.exe НЕ УДАЛАСЬ (см. $OUT/build.log)"
    exit 1
fi
grep -q 'warning:' "$OUT/build.log" && echo "  ВНИМАНИЕ: предупреждения сборки, см. $OUT/build.log"
cp "$SDL_DIR/bin/SDL2.dll" "$OUT/" || { echo "  нет $SDL_DIR/bin/SDL2.dll"; exit 1; }

# Путь для CreateProcessW: тест запускает хелпер сам, поэтому даём windows-путь.
WIN_OUT="Z:$(printf '%s' "$OUT" | sed 's#/#\\#g')"

echo "### прогон под Wine"
( cd "$OUT" && WINEDEBUG=-all timeout 300 wine unit_win.exe "$WIN_OUT\\win_argdump.exe" ) \
    >"$LOG" 2>&1
rc=$?
tr -d '\r' <"$LOG"

if [ "$rc" -eq 0 ]; then
    echo "  PASS: Windows-ветки (unit_win)"
else
    echo "  FAIL: unit_win вернул $rc (полный лог: $LOG)"
fi
exit "$rc"
