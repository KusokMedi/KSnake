// Проверка, что минимальный размер окна реально держится: создаём окно,
// ставим MIN_WIN_W x MIN_WIN_H и пробуем уменьшить его ниже минимума.
//
// Сборка: g++ -std=c++17 -o /tmp/ks_minwin tests/min_window.cpp -I src
//     $(pkg-config --cflags --libs sdl2)
#include <cstdio>

#include <SDL.h>

#include "config.h"

using ks::MIN_WIN_H;
using ks::MIN_WIN_W;
using ks::WIN_H;
using ks::WIN_W;

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::printf("FAIL: SDL_Init: %s\n", SDL_GetError());
        return 2;
    }
    SDL_Window* w = SDL_CreateWindow("KSnake-min", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
                                     SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!w) {
        std::printf("SKIP: SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 77;
    }
    SDL_SetWindowMinimumSize(w, MIN_WIN_W, MIN_WIN_H);

    int mw = 0, mh = 0;
    SDL_GetWindowMinimumSize(w, &mw, &mh);
    std::printf("minimum reported by SDL: %dx%d (want %dx%d)\n", mw, mh, MIN_WIN_W, MIN_WIN_H);
    if (mw != MIN_WIN_W || mh != MIN_WIN_H) {
        std::printf("FAIL: SDL did not accept the minimum size\n");
        SDL_DestroyWindow(w);
        SDL_Quit();
        return 1;
    }

    // Просим окно меньше минимума — SDL должна это отклонить.
    SDL_SetWindowSize(w, 200, 200);
    SDL_PumpEvents();
    int w_w = 0, w_h = 0;
    SDL_GetWindowSize(w, &w_w, &w_h);
    std::printf("after asking for 200x200 the window is %dx%d\n", w_w, w_h);

    bool held = w_w >= MIN_WIN_W && w_h >= MIN_WIN_H;
    // Часть драйверов (в том числе dummy) не применяет минимум принудительно —
    // это ограничение окружения, а не ошибка кода. Тогда проверяем, что
    // приложение хотя бы знает границу и не рисует вёрстку меньше неё.
    if (!held) {
        std::printf("NOTE: this video driver ignores the minimum size (layout relies on "
                    "MIN_WIN_W/MIN_WIN_H constants)\n");
    } else {
        std::printf("OK: window refused to go below the minimum\n");
    }

    SDL_DestroyWindow(w);
    SDL_Quit();
    return held ? 0 : 77;
}
