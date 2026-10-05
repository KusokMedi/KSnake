#!/usr/bin/env python3
"""Генерирует instrumented-копию исходников KSnake для визуальных проверок.

Копия `src/*.cpp` + `src/*.h` компилируется отдельно от рабочей сборки и
позволяет проверять экраны и размеры окна без кликов мышью:

  KS_SCREEN   принудительно открывает экран: menu, game, pause, gameover,
              credits, update, update_ok, update_ahead, update_check, update_fail
  KS_VER      подменяет app_version (состояния Available/UpToDate/Ahead, не
              трогая файл VERSION)
  KS_W/KS_H   размер окна на этапе генерации (WIN_W/WIN_H в config.h)
  KS_SHOT     префикс файла кадра, KS_SHOTS — номера кадров через запятую
  KS_SOFT     1 — рендерить offscreen ровно в WIN_W x WIN_H вместо окна

Запуск (генерация + сборка):

    KS_SOFT=1 KS_W=400 KS_H=360 KS_SCREEN=menu KS_VER=1.1 \
        KS_SHOT=tests/build/u.bmp KS_SHOTS=240 \
        python3 tests/make_test_source.py --build

    KS_SOFT=1 KS_SCREEN=menu KS_SHOT=tests/build/u.bmp KS_SHOTS=240 \
        setsid tests/build/snake_test --debug tests/build/run.log </dev/null \
        >/dev/null 2>&1 &
    sleep 8; pkill -x snake_test
    python3 tests/check_shot.py "tests/build/u.bmp.240.bmp"

Если в src/ изменились строки-якоря, скрипт падает с понятной ошибкой: молча
битая проверка недопустима, правь скрипт под новые якоря.
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")
OUT_DIR = os.path.join(ROOT, "tests", "build")
BIN = os.path.join(OUT_DIR, "snake_test")

# Экраны, которые можно открыть без ввода. Каждый — кусок кода на C++,
# выполняемый на первом кадре в теле главного цикла.
SCREENS = {
    "menu": "screen_state.screen = Screen::Menu;",
    "game": "screen_state.screen = Screen::Game;",
    "pause": "screen_state.screen = Screen::Game; paused = true;",
    "gameover": "screen_state.screen = Screen::Game; game_over_active = true;",
    "update": ("screen_state.screen = Screen::Update;"
               " updates.state_ = UpdateState::Available; updates.version_ = \"1.2.0\";"
               " updates.url_ = \"https://github.com/KusokMedi/KSnake/releases/tag/v1.2.0\";"),
    "update_ok": ("screen_state.screen = Screen::Update;"
                  " updates.state_ = UpdateState::UpToDate; updates.version_ = \"1.0.0\";"),
    "update_ahead": ("screen_state.screen = Screen::Update;"
                     " updates.state_ = UpdateState::Ahead; updates.version_ = \"1.0.0\";"),
    "update_check": "screen_state.screen = Screen::Update; updates.state_ = UpdateState::Checking;",
    "update_fail": ("screen_state.screen = Screen::Update; updates.state_ = UpdateState::Failed;"
                    " updates.note_ = \"Could not reach the GitHub releases API (HTTP 503)\";"),
}

# Детерминированная змейка для экранов игры: без неё поле всегда одинаковое
# и шаги не проверяются.
SNAKE_SETUP = """        game.snake_ = {{10, 7}, {9, 7}, {8, 7}, {8, 8}, {8, 9}};
        game.food_ = {14, 4};
        game.score_ = 7;
        // скорость 1 клетка в минуту: на статичном кадре змейка не доедет до стены,
        // поэтому поле проверяется независимо от момента съёмки
        game.move_interval_ = 60000;
"""

VERSION_HOOK = '    if (getenv("KS_VER")) app_version = getenv("KS_VER");\n'

# Компоновщик (Hyprland + XWayland) раздувает окно до 960x540, поэтому кадр в
# реальном окне не равен запрошенному KS_W/KS_H. KS_SOFT=1 переключает игру на
# offscreen software-рендер фиксированного размера: кадр тогда ровно WIN_W x WIN_H.
SOFT_RENDERER = """    SDL_Renderer* renderer = nullptr;
    SDL_Surface* test_surface = nullptr;
    if (getenv("KS_SOFT")) {
        test_surface = SDL_CreateRGBSurfaceWithFormat(0, WIN_W, WIN_H, 32,
                                                      SDL_PIXELFORMAT_ARGB8888);
        renderer = SDL_CreateSoftwareRenderer(test_surface);
        log_event("TEST software renderer %dx%d", WIN_W, WIN_H);
    } else {
        renderer = SDL_CreateRenderer(window, -1,
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer) {
            renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        }
    }
"""

LAYOUT_FIX = """        if (getenv("KS_SOFT")) {
            w = WIN_W;
            h = WIN_H;
        } else {
            SDL_GetWindowSize(window, &w, &h);
        }
"""

STATE_HOOK = """        {
            static int injected = 0;
            const char* forced = getenv("KS_SCREEN");
            if (injected == 0 && forced) {
                injected = 1;
                best_score = 9876;
                igt_ms = 83000;
""" + SNAKE_SETUP + """                if (std::string(forced) == "menu") {
                    """ + SCREENS["menu"] + """
                } else if (std::string(forced) == "game") {
                    """ + SCREENS["game"] + """
                } else if (std::string(forced) == "pause") {
                    """ + SCREENS["pause"] + """
                } else if (std::string(forced) == "gameover") {
                    """ + SCREENS["gameover"] + """
                } else if (std::string(forced) == "credits") {
                    screen_state.screen = Screen::Credits;
                } else if (std::string(forced) == "update") {
                    """ + SCREENS["update"] + """
                } else if (std::string(forced) == "update_ok") {
                    """ + SCREENS["update_ok"] + """
                } else if (std::string(forced) == "update_ahead") {
                    """ + SCREENS["update_ahead"] + """
                } else if (std::string(forced) == "update_check") {
                    """ + SCREENS["update_check"] + """
                } else if (std::string(forced) == "update_fail") {
                    """ + SCREENS["update_fail"] + """
                } else {
                    fprintf(stderr, "unknown KS_SCREEN=%s\\n", forced);
                }
            }
        }
"""

SHOT_HOOK = """        {
            static int sfc = 0;
            const char* path = getenv("KS_SHOT");
            const char* want = getenv("KS_SHOTS");
            bool take = false;
            if (path && want) {
                const char* p = want;
                while (*p) {
                    char* end = nullptr;
                    long v = strtol(p, &end, 10);
                    if (end == p) break;
                    if (v == sfc) take = true;
                    p = (*end == ',') ? end + 1 : end;
                    if (*end != ',') break;
                }
            }
            if (take) {
                std::string out = std::string(path) + "." + std::to_string(sfc) + ".bmp";
                SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, ui.lay_w(), ui.lay_h(), 32,
                                                                 SDL_PIXELFORMAT_ARGB8888);
                SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888, s->pixels,
                                     s->pitch);
                SDL_SaveBMP(s, out.c_str());
                SDL_FreeSurface(s);
            }
            ++sfc;
        }
"""


def patch_once(path, anchor, replacement, what):
    text = open(path, encoding="utf-8").read()
    count = text.count(anchor)
    if count != 1:
        sys.exit("make_test_source: якорь «%s» найден %d раз (нужен 1) в %s.\n"
                 "Обнови скрипт под текущий src/." % (what, count, os.path.basename(path)))
    open(path, "w", encoding="utf-8").write(text.replace(anchor, replacement, 1))


def generate():
    os.makedirs(OUT_DIR, exist_ok=True)
    copied = []
    for name in sorted(os.listdir(SRC)):
        if name.endswith((".cpp", ".h")):
            shutil.copyfile(os.path.join(SRC, name), os.path.join(OUT_DIR, name))
            if name.endswith(".cpp"):
                copied.append(name)
    # Собирается ровно то, что скопировано из src/: иначе любой посторонний
    # .cpp в tests/build (например, юнит-тест со своим main()) ломает линковку.
    with open(os.path.join(OUT_DIR, "sources.list"), "w", encoding="utf-8") as f:
        f.write("\n".join(copied) + "\n")

    width = os.environ.get("KS_W")
    height = os.environ.get("KS_H", "540")
    if width:
        patch_once(os.path.join(OUT_DIR, "config.h"), "constexpr int WIN_W = 960;",
                   "constexpr int WIN_W = %s;" % width, "WIN_W")
        patch_once(os.path.join(OUT_DIR, "config.h"), "constexpr int WIN_H = 540;",
                   "constexpr int WIN_H = %s;" % height, "WIN_H")

    # Тестовые копии: состояния обновлений и змейки форсируются напрямую,
    # поэтому у классов в копии приватные поля становятся публичными.
    patch_once(os.path.join(OUT_DIR, "update.h"), "\nprivate:\n    struct Fetch {",
               "\npublic:  // TEST: состояния форсируются напрямую\n    struct Fetch {", "private в update.h")
    patch_once(os.path.join(OUT_DIR, "game.h"), "\nprivate:\n    void place_food();",
               "\npublic:  // TEST: поле и очки форсируются напрямую\n    void place_food();",
               "private в game.h")

    main_cpp = os.path.join(OUT_DIR, "main.cpp")
    patch_once(main_cpp, "std::string app_version = load_app_version(&version_source);",
               "std::string app_version = load_app_version(&version_source);\n" + VERSION_HOOK,
               "load_app_version")
    patch_once(main_cpp, """    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1,
                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }""", SOFT_RENDERER.rstrip("\n"), "создание рендерера")
    patch_once(main_cpp, "        SDL_GetWindowSize(window, &w, &h);",
               LAYOUT_FIX.rstrip("\n"), "чтение размера окна в apply_window_layout")
    patch_once(main_cpp, "    while (running) {\n",
               "    while (running) {\n" + STATE_HOOK, "главный цикл")
    patch_once(main_cpp, "        SDL_RenderPresent(renderer);",
               SHOT_HOOK + "        SDL_RenderPresent(renderer);", "SDL_RenderPresent")


def build():
    with open(os.path.join(OUT_DIR, "sources.list"), encoding="utf-8") as f:
        sources = [os.path.join(OUT_DIR, name.strip()) for name in f if name.strip()]
    cflags = subprocess.check_output(["pkg-config", "--cflags", "sdl2"], text=True).split()
    libs = subprocess.check_output(["pkg-config", "--libs", "sdl2"], text=True).split()
    cmd = (["g++", "-std=c++17", "-I", OUT_DIR, "-I", os.path.join(ROOT, "third_party")]
           + cflags + sources + ["-o", BIN] + libs + ["-lSDL2_ttf", "-pthread"])
    print(" ".join(cmd))
    rc = subprocess.call(cmd, cwd=ROOT)
    if rc != 0:
        sys.exit("make_test_source: сборка тестовой копии не удалась")


if __name__ == "__main__":
    generate()
    if "--build" in sys.argv[1:]:
        build()
    else:
        print(OUT_DIR)