#!/usr/bin/env bash
# Headless-прогоны KSnake: память, дескрипторы, повторные запуски, выход
# во время фоновой проверки обновлений, запуск из другого каталога и по
# симлинку, отсутствующий/битый шрифт, пустой и непишемый HOME.
#
# Запуск:  ./tests/run_headless.sh [BINARY]
# По умолчанию берётся ./build/snake. Ничего не коммитится: логи в tests/build/.
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${1:-$ROOT/build/snake}"
OUT="$ROOT/tests/build/headless"
PREF="$HOME/.local/share/KSnake/KSnake"
mkdir -p "$OUT"

# HEADLESS_ONLY="1 2" — прогнать только эти секции.
want() { [ -z "${HEADLESS_ONLY:-}" ] || case " $HEADLESS_ONLY " in *" $1 "*) return 0;; *) return 1;; esac; }

pass=0; fail=0
ok()   { printf 'PASS: %s\n' "$*"; pass=$((pass+1)); }
bad()  { printf 'FAIL: %s\n' "$*"; fail=$((fail+1)); }
chk()  { if [ "$1" = "0" ]; then ok "$2"; else bad "$2 (rc=$1)"; fi }

echo "=== 0. окружение ==="
command -v "$BIN" >/dev/null 2>&1 || { echo "no binary: $BIN"; exit 2; }
echo "binary: $BIN"
echo "SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-<unset>}"

echo
echo "=== 1. 20 запусков подряд: код возврата и время выхода ==="
slow=0; badrc=0
for i in $(seq 1 20); do
    rm -f "$PREF/update_check.txt"
    t0=$(date +%s%N)
    SDL_VIDEODRIVER=dummy timeout -s INT -k 2 2 "$BIN" --debug "$OUT/run20.log" >/dev/null 2>&1
    rc=$?
    t1=$(date +%s%N)
    ms=$(( (t1 - t0) / 1000000 ))
    [ "$rc" = "124" ] || [ "$rc" = "130" ] || badrc=$((badrc+1))
    [ "$ms" -le 3000 ] || slow=$((slow+1))
    grep -q '^END ' "$OUT/run20.log" || { echo "  run $i: no END line"; badrc=$((badrc+1)); }
done
chk "$badrc" "20 запусков: нет аварийных кодов возврата и всегда есть строка END"
chk "$slow" "20 запусков: выход не дольше 3 с"

echo
if want 2; then
echo "=== 2. долгий прогон 300 с: RSS, число fd, размер кэша текста ==="
rm -f "$PREF/update_check.txt"
SDL_VIDEODRIVER=dummy "$BIN" --debug "$OUT/long.log" >/dev/null 2>&1 &
GPID=$!
for i in 1 2 3 4 5 6 7 8 9 10; do
    sleep 30
    if ! kill -0 "$GPID" 2>/dev/null; then break; fi
    rss=$(awk '/VmRSS/{print $2}' "/proc/$GPID/status" 2>/dev/null)
    fds=$(ls "/proc/$GPID/fd" 2>/dev/null | wc -l)
    thr=$(awk '/Threads/{print $2}' "/proc/$GPID/status" 2>/dev/null)
    echo "  t=$((i*30))s rss=${rss:-?}kB fds=$fds threads=$thr"
    echo "$rss $fds" >> "$OUT/long.samples"
done
kill "$GPID" 2>/dev/null; wait "$GPID" 2>/dev/null
if [ -s "$OUT/long.samples" ]; then
    first_rss=$(head -1 "$OUT/long.samples" | cut -d' ' -f1)
    last_rss=$(tail -1 "$OUT/long.samples" | cut -d' ' -f1)
    first_fd=$(head -1 "$OUT/long.samples" | cut -d' ' -f2)
    last_fd=$(tail -1 "$OUT/long.samples" | cut -d' ' -f2)
    echo "  RSS: ${first_rss}kB -> ${last_rss}kB ; fd: $first_fd -> $last_fd"
    [ "$((last_rss - first_rss))" -lt 4096 ]; chk $? "RSS не растёт больше чем на 4 МБ за 5 мин"
    [ "$((last_fd - first_fd))" -le 2 ]; chk $? "число открытых fd стабильно"
else
    bad "не удалось собрать samples долгого прогона"
fi
fi

echo
if want 3; then
echo "=== 3. сигналы и выход во время фоновой проверки обновлений ==="
for sig in TERM INT; do
    rm -f "$PREF/update_check.txt"
    t0=$(date +%s%N)
    SDL_VIDEODRIVER=dummy timeout -s "$sig" -k 2 1 "$BIN" --debug "$OUT/sig_$sig.log" >/dev/null 2>&1
    rc=$?; t1=$(date +%s%N); ms=$(( (t1 - t0) / 1000000 ))
    left=$(pgrep -x snake | wc -l)
    echo "  SIG$sig -> rc=$rc за ${ms}ms, процессов осталось: $left"
    [ "$rc" = "124" ] && [ "$ms" -le 3000 ] && [ "$left" = "0" ] \
        && ok "SIG$sig: игра завершается за ${ms}ms, процессов не осталось" \
        || bad "SIG$sig: rc=$rc ${ms}ms, осталось процессов $left"
    grep -q '^END ' "$OUT/sig_$sig.log"; chk $? "SIG$sig: корректный выход (строка END)"
done

# Медленный curl: игра выходит, пока запрос ещё идёт.
# timeout шлёт сигнал ProcessGroup, поэтому игра и её потомки умирают вместе и
# тест ничего не доказывает. Нужен точечный сигнал только PID игры, причём
# SIGINT/SIGQUIT не должны наследовать SIG_IGN от фоновой задачи шелла.
STUB=/tmp/ks_stub_inflight
rm -rf "$STUB"; mkdir -p "$STUB"
printf '#!/bin/sh\n/bin/sleep 25\nexit 0\n' > "$STUB/curl"; chmod +x "$STUB/curl"
rm -f "$PREF/update_check.txt"
PATH="$STUB:$PATH" SDL_VIDEODRIVER=dummy python3 -c '
import os, signal, sys
for s in (signal.SIGINT, signal.SIGQUIT, signal.SIGHUP):
    signal.signal(s, signal.SIG_DFL)
os.execv(sys.argv[1], sys.argv[1:])' "$BIN" --debug "$OUT/exit_inflight.log" >/dev/null 2>&1 &
GAME=$!
sleep 1.2
kill -TERM "$GAME"
wait "$GAME" 2>/dev/null; rc=$?
sleep 0.4
orph=$(ps -eo pid,ppid,stat,args= | grep -F "$STUB/curl" | grep -v grep | wc -l)
echo "  в полёте curl: rc=$rc, осиротевших процессов после выхода: $orph"
[ "$rc" = "0" ]; chk $? "выход во время проверки не ждёт и не падает (rc=$rc)"
[ "$orph" = "0" ]; chk $? "при выходе во время проверки curl не остаётся сиротой"
ps -eo pid,args= | grep -F "$STUB/curl" | grep -v grep | awk '{print $1}' | xargs -r kill -9 2>/dev/null
rm -rf "$STUB"
fi

echo
if want 4; then
echo "=== 4. запуск из другого каталога ==="
rm -f "$PREF/update_check.txt"
( cd /tmp && SDL_VIDEODRIVER=dummy timeout 3 "$BIN" --debug "$OUT/othercwd.log" >/dev/null 2>&1 )
f=$(grep -m1 '^FONT path=' "$OUT/othercwd.log" | sed 's/.*path=//; s/ size.*//')
echo "  из /tmp выбран шрифт: ${f:-<нет, игра не стартовала>}"
case "$f" in
    "") bad "из /tmp шрифт не найден вовсе" ;;
    assets/font.ttf) bad "из /tmp выбран относительный assets/font.ttf (его там нет)" ;;
    *) ok "из /tmp шрифт найден через FONT_CANDIDATES: $f" ;;
esac
grep -q '^END ' "$OUT/othercwd.log"; chk $? "из /tmp игра дошла до главного цикла (есть END)"
grep -q '^VERSION .* from ' "$OUT/othercwd.log" && ok "VERSION прочитан из /tmp" \
    || bad "VERSION не прочитан из /tmp"
fi

echo
if want 5; then
echo "=== 5. запуск по симлинку ==="
rm -f "$PREF/update_check.txt"; rm -f /tmp/ks_link
ln -s "$BIN" /tmp/ks_link
( cd / && SDL_VIDEODRIVER=dummy timeout 3 /tmp/ks_link --debug "$OUT/symlink.log" >/dev/null 2>&1 )
grep -q '^FONT path=' "$OUT/symlink.log"; chk $? "через симлинк шрифт найден"
grep -q 'VERSION' "$OUT/symlink.log"; chk $? "через симлинк VERSION прочитан (SDL_GetBasePath)"
rm -f /tmp/ks_link
fi

echo
if want 6; then
echo "=== 6. нет ни одного шрифта (FONT_CANDIDATES пуст) ==="
# Собирам временную копию с пустым массивом кандидатов.
STAGE=/tmp/ks_nofont
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -r "$ROOT/src" "$STAGE/src"
python3 - "$STAGE/src/config.h" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
i = s.index('constexpr const char* FONT_CANDIDATES[] = {')
j = s.index('};', i)
open(p, 'w').write(s[:i] + 'constexpr const char* FONT_CANDIDATES[] = {\n    "/nonexistent/font.ttf",\n' + s[j:])
PY
g++ -std=c++17 $(pkg-config --cflags sdl2) -Ithird_party "$STAGE"/src/*.cpp \
    -o "$STAGE/snake" $(pkg-config --libs sdl2) -lSDL2_ttf -pthread 2>/dev/null
rm -f "$PREF/update_check.txt"
SDL_VIDEODRIVER=dummy timeout 5 "$STAGE/snake" --debug "$OUT/nofont.log" >"$OUT/nofont.out" 2>&1
rc=$?
echo "  rc=$rc stderr: $(head -2 "$OUT/nofont.out" | tr '\n' ' ')"
{ [ "$rc" != "124" ] && [ "$rc" != "0" ]; }; chk $? "нет шрифта -> понятный ненулевой выход (rc=$rc), не зависание"
grep -q 'No usable font' "$OUT/nofont.out"; chk $? "нет шрифта: в stderr есть понятное сообщение"
fi

echo
if want 7; then
echo "=== 7. битый шрифт ==="
STAGE2=/tmp/ks_badfont
rm -rf "$STAGE2"; mkdir -p "$STAGE2/assets"
head -c 4096 /dev/urandom > "$STAGE2/assets/font.ttf"
cp -r "$ROOT/src" "$STAGE2/src"
g++ -std=c++17 $(pkg-config --cflags sdl2) -Ithird_party "$STAGE2"/src/*.cpp \
    -o "$STAGE2/snake" $(pkg-config --libs sdl2) -lSDL2_ttf -pthread 2>/dev/null
rm -f "$PREF/update_check.txt"
( cd "$STAGE2" && SDL_VIDEODRIVER=dummy timeout 5 ./snake --debug "$OUT/badfont.log" >/dev/null 2>&1 )
rc=$?
f=$(grep -m1 '^FONT path=' "$OUT/badfont.log" | sed 's/.*path=//; s/ size.*//')
echo "  rc=$rc выбран шрифт: ${f:-<ни один, игра не стартовала>}"
if [ "$f" = "assets/font.ttf" ]; then
    bad "битый assets/font.ttf принят как валидный шрифт"
elif [ -n "$f" ]; then
    ok "битый assets/font.ttf отвергнут, взят системный: $f"
else
    ok "битый assets/font.ttf отвергнут и всеми кандидатами не найдено"
fi
fi

echo
if want 8; then
echo "=== 8. пустой HOME ==="
rm -f "$PREF/update_check.txt"
HOME=/nonexistent-home SDL_VIDEODRIVER=dummy timeout 5 "$BIN" --debug "$OUT/emptyhome.log" >/dev/null 2>&1
rc=$?
grep -q '^BEST ' "$OUT/emptyhome.log" && b=$(grep -m1 '^BEST ' "$OUT/emptyhome.log" | sed 's/.*path=//') || b="<нет>"
echo "  rc=$rc pref dir: $b"
# 124 = timeout убил живую игру через 5 с, это и есть успех.
{ [ "$rc" = "124" ] || [ "$rc" = "0" ]; }; chk $? "пустой HOME -> игра не падает (rc=$rc)"
grep -q '^END ' "$OUT/emptyhome.log"; chk $? "пустой HOME: корректный выход по таймауту"
fi

echo
if want 9; then
echo "=== 9. HOME только для чтения ==="
RO=/tmp/ks_ro_home
rm -rf "$RO"; mkdir -p "$RO/.local/share/KSnake"
cp "$PREF/scores.json" "$RO/.local/share/KSnake/scores.json" 2>/dev/null
chmod -R a-w "$RO"
rm -f "$RO/.local/share/KSnake/update_check.txt"
HOME="$RO" SDL_VIDEODRIVER=dummy timeout 5 "$BIN" --debug "$OUT/rohome.log" >/dev/null 2>&1
rc=$?
echo "  rc=$rc"
{ [ "$rc" = "124" ] || [ "$rc" = "0" ]; }; chk $? "HOME только для чтения -> игра не падает (rc=$rc)"
grep -q '^END ' "$OUT/rohome.log"; chk $? "HOME только для чтения: корректный выход по таймауту"
chmod -R u+w "$RO"; rm -rf "$RO"
fi

echo
if want 10; then
echo "=== 10. дескрипторы и потоки в логе долгого прогона ==="
grep -c '^STEP ' "$OUT/long.log" 2>/dev/null || echo 0
echo "--- thread-related строки в логе ---"
grep -iE 'thread|fork|curl' "$OUT/long.log" 2>/dev/null | head
fi

echo
echo "=== итог: pass=$pass fail=$fail ==="
[ "$fail" = "0" ]