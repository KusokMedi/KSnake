#!/usr/bin/env bash
# Фаззинг save_best_score(): SIGKILL в случайный момент записи.
#
# Раньше прямая запись в scores.json давала 164 битых файла из 3000 прогонов.
# С атомарной записью (temp + fsync + rename) файл рекорда обязан остаться
# целым при любом kill. Скрипт гоняет цикл сохранений, убивает процесс в
# случайный момент и проверяет, что рекорд читается.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN=/tmp/ks_fuzz_save
DIR=/tmp/ks_fuzz_dir
ITER=${1:-400}
OK=0
BAD=0
STRAY=0

cd "$ROOT" || exit 1
g++ -std=c++17 -O1 -o "$BIN" tests/fuzz_save.cpp src/storage.cpp src/util.cpp src/version.cpp \
    -I src -I third_party $(pkg-config --cflags --libs sdl2) -lpthread || exit 1

rm -rf "$DIR"
mkdir -p "$DIR"

for ((i = 1; i <= ITER; i++)); do
    rm -f "$DIR"/scores.json "$DIR"/scores.json.tmp*
    # Первый валидный файл: kill не должен суметь оставить хуже, чем целый JSON.
    "$BIN" "$DIR/scores.json" save 1 >/dev/null 2>&1

    "$BIN" "$DIR/scores.json" save 100000 >/dev/null 2>&1 &
    pid=$!
    # Случайная точка kill внутри окна записи (write + fsync занимают миллисекунды).
    python3 -c "import random,time,sys; time.sleep(random.uniform(0.0005, 0.030)); sys.stdout.flush()" >/dev/null 2>&1
    kill -9 "$pid" 2>/dev/null
    wait "$pid" 2>/dev/null

    out=$("$BIN" "$DIR/scores.json" check 2>/dev/null)
    if [[ $? -eq 0 && "$out" =~ ^[0-9]+$ && "$out" -ge 1 ]]; then
        OK=$((OK + 1))
    else
        BAD=$((BAD + 1))
        echo "  ИТЕРАЦИЯ $i: битый рекорд, прочитано '${out:-<пусто>}'"
        cp -f "$DIR/scores.json" "/tmp/ks_fuzz_bad_$i.json" 2>/dev/null
    fi
    # Временный файл после kill — это нормально. Важно, что он один и тот же:
    # мусор не должен копиться. Поэтому меряем максимум одновременных файлов.
    n=$(find "$DIR" -name 'scores.json.tmp*' 2>/dev/null | wc -l)
    if ((n > STRAY)); then STRAY=$n; fi
done

echo "fuzz_save: ok=$OK bad=$BAD (из $ITER), максимум временных файлов: $STRAY"
rm -rf "$DIR" "$BIN"
[[ $BAD -eq 0 ]]
