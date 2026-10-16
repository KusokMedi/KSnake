# Testing KSnake

The test tooling lives in `tests/` and is deliberately **not** part of the CMake build or
the release artifacts. All generated output goes to `tests/build/`, which is git-ignored.
The repository policy for running and reporting checks is defined in
[root `AGENTS.md`](../AGENTS.md) (Section 8); this document is the concrete procedure.

## Prerequisites

- Build toolchain and SDL2/SDL2_ttf development packages (see the README requirements).
- `pkg-config` — used by the test build commands.
- Python 3 with [Pillow](https://python-pillow.org/) for `tests/check_shot.py`
  (it exits with `check_shot: нужен Pillow` when missing).
- [Tesseract](https://github.com/tesseract-ocr/tesseract) — optional; enables the OCR
  line in `check_shot.py`. Without it that line reads `(tesseract не установлен)`.
- `curl` — used by the update subsystem and the headless harness.

## 1. Warning-focused build

A clean out-of-tree build with warnings enabled. `CMakeLists.txt` already adds
`-Wall -Wextra`, and the explicit flag makes the check independent of that.

```bash
rm -rf /tmp/ksb
cmake -S . -B /tmp/ksb -DCMAKE_CXX_FLAGS="-Wall -Wextra"
cmake --build /tmp/ksb
```

Expected: configuration and build exit `0` with zero warnings. (Verified: 0 warnings.)

## 2. Headless smoke test

```bash
SDL_VIDEODRIVER=dummy timeout 5 /tmp/ksb/snake --debug /tmp/snake.log >/dev/null
grep '^END ' /tmp/snake.log
```

Expected output includes `END score=0 screen=0`. `timeout` stops the window-less main
loop, so the command returns `124`; SDL's default signal handling makes the game exit
cleanly and write the `END` line. The log also contains `WIN minimum=400x360`,
`VERSION ... from cwd`, `RENDERER driver=software` and `FONT path=...` lines.

This test only proves that startup and teardown neither hang nor crash. It does **not**
validate layout or visuals — use the visual matrix for that.

## 3. Unit tests

There are three standalone test binaries. Each prints `unit_<name>: pass=N fail=N` and
returns non-zero when anything fails.

```bash
mkdir -p tests/build

# Game logic (game.cpp has no SDL dependency)
g++ -std=c++17 -O1 -Wall -Wextra -I src -I third_party \
    tests/unit_game.cpp src/game.cpp -o tests/build/unit_game

# util + version + storage (needs SDL for SDL_GetPrefPath/SDL_GetBasePath)
g++ -std=c++17 -O1 -Wall -Wextra -I src -I third_party \
    tests/unit_util.cpp src/util.cpp src/version.cpp src/storage.cpp \
    -o tests/build/unit_util $(pkg-config --cflags --libs sdl2) -lSDL2_ttf -pthread

# Update logic (uses a stub curl on PATH, see below)
g++ -std=c++17 -O1 -Wall -Wextra -I src -I third_party \
    tests/unit_update.cpp src/update.cpp src/util.cpp src/version.cpp \
    -o tests/build/unit_update $(pkg-config --cflags --libs sdl2) -lSDL2_ttf -pthread

SDL_VIDEODRIVER=dummy tests/build/unit_game
SDL_VIDEODRIVER=dummy tests/build/unit_util
SDL_VIDEODRIVER=dummy tests/build/unit_update
```

Observed results with the current sources: `unit_game` 98/0, `unit_util` 126/0,
`unit_update` 72/0.

- `unit_game` exercises movement, queued turns, wall/self collisions, growth, food
  spawning, speed bounds, determinism and the win-by-filling-the-field path.
- `unit_update` substitutes a script named `curl` early on `PATH`, so `http_get` reads
  local files instead of touching the network; it covers cache restore, the 5-minute
  freshness window, the 8-hour background cycle, poll transitions, `mark_checked` and
  the glow animation.
- `unit_util` covers `parse_version`, `format_version`, `compare_versions`, `fnv1a`,
  `trim_str`, version-file loading and the signed `scores.json` format, and runs
  sub-process checks for the preference directory.

> **Known discrepancy:** the `tests/unit_*.cpp` header comments point at
> `./tests/build_unit.sh`. That script is not present in the repository; build the unit
> tests with the commands above until it is restored or the comments are updated.

## 4. Headless robustness harness

`tests/run_headless.sh` runs the game with `SDL_VIDEODRIVER=dummy` and checks resource
usage, signal handling and degraded filesystem conditions.

```bash
./tests/run_headless.sh                # uses ./build/snake by default
./tests/run_headless.sh /tmp/ksb/snake # or pass an explicit binary
HEADLESS_ONLY="1 3" ./tests/run_headless.sh   # run only sections 1 and 3
```

It writes logs to `tests/build/headless/` and ends with `=== итог: pass=N fail=M ===`.
Sections:

1. 20 consecutive startups — exit codes and time-to-exit (must not exceed 3 s).
2. 300 s long run — RSS and open file-descriptor counts must stay flat.
3. `SIGTERM`/`SIGINT` during a background update check, plus a slow `curl` stub that
   proves the game does not leave an orphaned `curl` behind.
4. Starting from another working directory (font lookup).
5. Starting through a symlink (`SDL_GetBasePath`).
6. No usable font (`FONT_CANDIDATES` all invalid) — must fail with a clear message.
7. Corrupted `assets/font.ttf` — must be rejected in favour of a system font.
8. Empty `HOME`.
9. Read-only `HOME`.
10. Log inspection: step counter and thread/`curl`-related lines from the long run.

> The harness uses the real preference directory `$HOME/.local/share/KSnake/KSnake`
> and deletes `update_check.txt` there; section 9 copies `scores.json`. Do not point it
> at data you cannot reproduce.

## 5. Visual matrix

`tests/run_visual_matrix.sh` renders every screen at several window sizes using the
instrumented build and checks each frame with `check_shot.py`.

```bash
./tests/run_visual_matrix.sh                          # default sizes
./tests/run_visual_matrix.sh "400x360 960x540"        # custom sizes
```

Default sizes: `400x360 640x480 960x540 1280x720 1920x1080 3840x2160`.
Screens: `menu game pause gameover credits update update_ok update_ahead update_check
update_fail`.

For each combination it requires `edge=0` (no light pixels on the frame boundary) and
that the captured BMP dimensions equal the requested size. It writes BMPs and logs to
`tests/build/visual/` and ends with `=== визуальная матрица: pass=N fail=M ===`.

`check_shot.py` can also be run directly on a single frame:

```bash
python3 tests/check_shot.py tests/build/visual/960x540_menu.bmp.240.bmp
```

It prints the frame size and `edge` count, the bounding box of the `(26,26,26)` panels,
the amber-pixel count for the `Check for updates` button and, when Tesseract is present,
the recognized text. Its exit code is always `0`; it is meant to be read, not used as a
CI gate.

## 6. Visual harness internals

`tests/make_test_source.py` copies `src/*.cpp` and `src/*.h` into `tests/build/`, patches
the copies with test hooks and (with `--build`) compiles `tests/build/snake_test`. It
fails loudly if an anchor line in `src/` changed, so a source refactor that breaks the
harness must be fixed by updating the anchors, not by skipping the check.

Environment variables:

| Variable | Meaning |
|---|---|
| `KS_SCREEN` | force a screen (see list below) |
| `KS_VER` | override `app_version` (drives Available / UpToDate / Ahead) |
| `KS_W`, `KS_H` | compile-time window size (`WIN_W`/`WIN_H` in `config.h`) |
| `KS_SHOT` | BMP filename prefix |
| `KS_SHOTS` | comma-separated frame numbers to capture |
| `KS_SOFT` | `1` renders offscreen at exactly `WIN_W x WIN_H` via `SDL_CreateSoftwareRenderer` |

`KS_SCREEN` values: `menu`, `game`, `pause`, `gameover`, `credits`, `update`,
`update_ok`, `update_ahead`, `update_check`, `update_fail`.

`KS_SOFT=1` must be set for deterministic dimension checks: on a real window the
compositor (e.g. Hyprland through XWayland) may resize the window to 960x540, so the
captured frame would not match the requested size. Compile-time dimensions of interest
are 400x360, 960x540 and 1920x1080. The matrix uses frame 240 (frame 0 can be torn).

## 7. Save-atomicity fuzzer

`tests/fuzz_save.sh` builds `tests/fuzz_save.cpp` with `src/storage.cpp` and repeatedly
kills the writer with `SIGKILL` at a random moment; the score file must stay readable.

```bash
./tests/fuzz_save.sh           # 400 iterations (default)
./tests/fuzz_save.sh 1000      # custom count
```

Expected: `fuzz_save: ok=<iterations> bad=0`. Temporary files live in `/tmp` and are
removed afterwards.

`tests/slow_save.c` is an optional `LD_PRELOAD` helper that widens the vulnerable window
by sleeping inside `open(..., O_TRUNC)`:

```bash
g++ -shared -fPIC -O1 -Wall -Wextra -o /tmp/slowsave.so tests/slow_save.c -ldl
LD_PRELOAD=/tmp/slowsave.so ./build/snake --debug /tmp/slow.log
```

## 8. Minimum window size

```bash
g++ -std=c++17 -I src tests/min_window.cpp -o /tmp/ks_minwin $(pkg-config --cflags --libs sdl2)
/tmp/ks_minwin
```

Exit codes: `0` — the driver enforced the 400x360 minimum; `77` — the driver (for
example the dummy driver) ignores the minimum, in which case the layout still clamps
internally; `1` — SDL rejected the minimum. Verified: `OK: window refused to go below
the minimum`.

## Output and cleanup

Everything generated by these procedures lives under `tests/build/` (git-ignored) or in
`/tmp`; never commit it. `fresh-start.sh` only removes the build directory and does not
touch the test output.
