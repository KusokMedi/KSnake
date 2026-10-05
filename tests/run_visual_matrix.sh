#!/usr/bin/env bash
# Визуальная матрица KSnake: все экраны на всех размерах окна.
#
# Требования AGENTS.md:
#   * KS_SOFT=1 обязателен, иначе компоновщик растянет окно и кадр будет 960x540;
#   * KS_W/KS_H читает генератор (WIN_W/WIN_H в config.h), остальное — бинарь;
#   * генерация, сборка и прогон — отдельными командами;
#   * кадр 0 может быть битым, его не проверяем (берём кадр 240);
#   * кэш проверки обновлений чистим до и после серии.
#
# Запуск:  ./tests/run_visual_matrix.sh [список размеров через пробел]
# Размеры по умолчанию: 400x360 640x480 960x540 1280x720 1920x1080 3840x2160
# Размеры меньше 400x360 в список не входят: игра держит минимум MIN_WIN_W x
# MIN_WIN_H через SDL_SetWindowMinimumSize на всех бэкендах, поэтому состояние
# 200x200 недостижимо, а offscreen-кадр такого размера проверял бы вёрстку,
# которой в игре быть не может.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2
OUT="$ROOT/tests/build/visual"
PREF="$HOME/.local/share/KSnake/KSnake"
SHOT_N=240

SIZES=${*:-"400x360 640x480 960x540 1280x720 1920x1080 3840x2160"}
SCREENS="menu game pause gameover credits update update_ok update_ahead update_check update_fail"

rm -rf "$OUT"; mkdir -p "$OUT"
rm -f "$PREF/update_check.txt"

pass=0; fail=0
bad_list=""

for size in $SIZES; do
    W=${size%x*}; H=${size#*x}
    echo "### размер ${W}x${H}"
    KS_SOFT=1 KS_W="$W" KS_H="$H" python3 tests/make_test_source.py --build >"$OUT/build_$size.log" 2>&1
    if [ ! -x tests/build/snake_test ]; then
        echo "  СБОРКА НЕ УДАЛАСЬ (см. $OUT/build_$size.log)"; fail=$((fail+1))
        bad_list="$bad_list build:$size"
        continue
    fi
    for scr in $SCREENS; do
        # Свежий кэш: фоновая проверка откладывается на 8 ч, и форсированное
        # состояние экрана не перебивается результатом реального запроса.
        printf '%s 4 1.1 \n' "$(date +%s)" > "$PREF/update_check.txt"
        base="$OUT/${size}_${scr}"
        rm -f "$base".*
        # Ждём только кадра, потом сразу снимаем процесс: на 1920x1080
        # софтверный рендер медленный, ждать таймаут на каждом прогоне нельзя.
        KS_SOFT=1 KS_SCREEN="$scr" KS_VER=1.1 \
            KS_SHOT="$base.bmp" KS_SHOTS="$SHOT_N" \
            setsid tests/build/snake_test --debug "$OUT/$size-$scr.log" \
            </dev/null >/dev/null 2>&1 &
        spid=$!
        waited=0
        while [ ! -f "$base.bmp.$SHOT_N.bmp" ] && [ "$waited" -lt 1200 ]; do
            sleep 0.2; waited=$((waited+1))
            kill -0 "$spid" 2>/dev/null || break
        done
        pkill -x snake_test 2>/dev/null
        wait "$spid" 2>/dev/null
        if [ ! -f "$base.bmp.$SHOT_N.bmp" ]; then
            echo "  FAIL $scr: кадр $SHOT_N не сохранён"; fail=$((fail+1))
            bad_list="$bad_list $size/$scr:no-shot"; continue
        fi
        line=$(python3 tests/check_shot.py "$base.bmp.$SHOT_N.bmp" 2>&1)
        edge=$(printf '%s' "$line" | sed -n 's/.*edge=\([0-9]*\).*/\1/p' | head -1)
        dims=$(printf '%s' "$line" | sed -n 's/.*bmp  \([0-9]*x[0-9]*\)  edge=.*/\1/p' | head -1)
        txt=$(printf '%s' "$line" | sed -n 's/^   текст: //p' | head -1)
        status="PASS"
        if [ "$edge" != "0" ]; then status="FAIL"; fi
        if [ "$dims" != "${W}x${H}" ]; then status="FAIL"; fi
        if [ "$status" = "PASS" ]; then
            pass=$((pass+1))
        else
            fail=$((fail+1)); bad_list="$bad_list $size/$scr(edge=$edge,dims=$dims)"
        fi
        printf '  %-4s %-13s %-9s edge=%-4s %s\n' "$status" "$scr" "${dims:-?}" "${edge:-?}" "${txt:0:110}"
    done
done

rm -f "$PREF/update_check.txt"
echo
echo "=== визуальная матрица: pass=$pass fail=$fail ==="
[ -n "$bad_list" ] && echo "не прошли:$bad_list"
[ "$fail" = "0" ]