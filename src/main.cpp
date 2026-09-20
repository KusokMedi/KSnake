#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <SDL_ttf.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <random>
#include <string>
#include <utility>

using json = nlohmann::json;

namespace {
constexpr int CELL = 40;
constexpr int WIDTH = 20;
constexpr int HEIGHT = 15;
constexpr int WIN_W = 960;
constexpr int WIN_H = 540;
constexpr int FPS = 60;

constexpr const char* FONT_CANDIDATES[] = {
    "assets/font.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/seguisb.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
};

constexpr int MIN_WIN_W = 400;
constexpr int MIN_WIN_H = 360;

uint32_t fnv1a(const std::string& s) {
    uint32_t h = 2166136261u;
    for (char c : s) {
        h ^= static_cast<uint8_t>(c);
        h *= 16777619u;
    }
    return h;
}
}  // namespace

struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

struct CreditLink {
    const char* label;
    const char* url;
    int dy;  // offset from window centre
};

constexpr CreditLink CREDIT_LINKS[] = {
    {"My github: ", "https://github.com/kusokmedi", -40},
    {"Source code: ", "https://github.com/KusokMedi/KSnake", -12},
    {"Telegram: ", "https://t.me/kusokmedi52", 16},
    {"About me: ", "https://kusokmedi.lat", 44},
};
constexpr int CREDIT_LINK_COUNT = sizeof(CREDIT_LINKS) / sizeof(CREDIT_LINKS[0]);

enum class Screen { Menu, Game, ExitPrompt, Credits };

int main(int argc, char* argv[]) {
    FILE* log_fp = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--debug") == 0 && i + 1 < argc) {
            log_fp = std::fopen(argv[++i], "w");
            if (!log_fp) {
                fprintf(stderr, "Cannot open debug log: %s\n", argv[i]);
            }
        }
    }

    auto log_event = [&](const char* fmt, ...) {
        if (!log_fp) return;
        va_list args;
        va_start(args, fmt);
        vfprintf(log_fp, fmt, args);
        va_end(args);
        fprintf(log_fp, "\n");
        fflush(log_fp);
    };

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        if (log_fp) std::fclose(log_fp);
        return -1;
    }
    if (TTF_Init() < 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        SDL_Quit();
        if (log_fp) std::fclose(log_fp);
        return -1;
    }

    #ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "1");
#endif

    SDL_Window* window =
        SDL_CreateWindow("KSnake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         WIN_W, WIN_H,
                         SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return -1;
    }
    // SDL_SetWindowMinimumSize is applied on the first frame.
    // On the Wayland backend applying it at creation shrinks the window
    // down to the minimum size (SDL behavior), so Wayland keeps no
    // hard minimum; the layout is clamped internally instead.

    int init_w = 0, init_h = 0;
    SDL_GetWindowSize(window, &init_w, &init_h);
    log_event("WIN w=%d h=%d", init_w, init_h);
    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1,
                           SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return -1;
    }

    SDL_RendererInfo rinfo;
    if (SDL_GetRendererInfo(renderer, &rinfo) == 0) {
        log_event("RENDERER driver=%s", rinfo.name);
    }

    auto load_font = [&](int size) -> TTF_Font* {
        for (const char* path : FONT_CANDIDATES) {
            TTF_Font* f = TTF_OpenFont(path, size);
            if (f) {
                log_event("FONT path=%s size=%d", path, size);
                return f;
            }
        }
        return nullptr;
    };
    TTF_Font* font_title = load_font(64);
    TTF_Font* font_button = load_font(26);
    TTF_Font* font_small = load_font(16);
    TTF_Font* font_gameover = load_font(48);
    if (!font_title || !font_button || !font_small || !font_gameover) {
        fprintf(stderr, "No usable font found. Tried:\n");
        for (const char* path : FONT_CANDIDATES) {
            fprintf(stderr, "  %s\n", path);
        }
        log_event("FATAL no usable font found");
        if (font_title) TTF_CloseFont(font_title);
        if (font_button) TTF_CloseFont(font_button);
        if (font_small) TTF_CloseFont(font_small);
        if (font_gameover) TTF_CloseFont(font_gameover);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        if (log_fp) std::fclose(log_fp);
        return -1;
    }

    std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    std::uniform_int_distribution<int> dist_w(0, WIDTH - 1);
    std::uniform_int_distribution<int> dist_h(0, HEIGHT - 1);

    std::deque<Point> snake = {{WIDTH / 2, HEIGHT / 2},
                               {WIDTH / 2 - 1, HEIGHT / 2},
                               {WIDTH / 2 - 2, HEIGHT / 2}};
    int dx = 1, dy = 0;
    Point food{WIDTH / 2 + 5, HEIGHT / 2};
    int score = 0;
    Uint32 igt_ms = 0;   // in-game time, excludes paused/game-over time
    std::deque<std::pair<int, int>> pending_turns;
    Uint32 move_acc = 0;
    Uint32 move_interval = 150;
    bool paused = false;
    bool game_over_active = false;
    bool game_won_active = false;
    Uint32 over_ms = 0;  // tick when game over / won activated

    auto place_food = [&]() {
        if (static_cast<int>(snake.size()) >= WIDTH * HEIGHT) return;
        do {
            food = {dist_w(rng), dist_h(rng)};
        } while (std::find(snake.begin(), snake.end(), food) != snake.end());
    };

    auto reset_game = [&]() {
        snake = {{WIDTH / 2, HEIGHT / 2},
                 {WIDTH / 2 - 1, HEIGHT / 2},
                 {WIDTH / 2 - 2, HEIGHT / 2}};
        dx = 1;
        dy = 0;
        pending_turns.clear();
        score = 0;
        igt_ms = 0;
        move_acc = 0;
        move_interval = 150;
        game_over_active = false;
        game_won_active = false;
        over_ms = 0;
        place_food();
    };
    place_food();

    auto queue_turn = [&](int ndx, int ndy) {
        int ex = dx, ey = dy;
        if (!pending_turns.empty()) {
            ex = pending_turns.back().first;
            ey = pending_turns.back().second;
        }
        if (ndx == ex && ndy == ey) return;
        if (ndx == -ex && ndy == -ey) return;
        if (pending_turns.size() < 3) pending_turns.push_back({ndx, ndy});
    };

    Screen screen = Screen::Menu;
    int menu_index = 0;
    bool menu_coming[3] = {false, false, false};
    Uint32 menu_coming_start[3] = {0, 0, 0};
    Uint32 menu_coming_until[3] = {0, 0, 0};
    int credit_hover = -1;

    int cell = CELL;
    int offset_x = 0;
    int offset_y = 0;

    auto compute_layout = [&](int win_w, int win_h) {
        if (win_w < 1) win_w = 1;
        if (win_h < 1) win_h = 1;
        int pad = 16;
        cell = std::min((win_w - 2 * pad) / WIDTH, (win_h - 2 * pad) / HEIGHT);
        if (cell < 1) cell = 1;
        offset_x = (win_w - cell * WIDTH) / 2;
        offset_y = (win_h - cell * HEIGHT) / 2;
        if (offset_x < pad) offset_x = pad;
        if (offset_y < pad) offset_y = pad;
    };

    {
        int start_w = 0, start_h = 0;
        SDL_GetWindowSize(window, &start_w, &start_h);
        compute_layout(start_w, start_h);
    }

    struct MenuLayout {
        SDL_Rect buttons[5];
        int title_cy;
    };
    auto menu_layout = [&](int win_w, int win_h) -> MenuLayout {
        int margin = 8;
        int btn_h = 48;
        int gap = 20;
        int head_step = 80;  // title centre -> first button centre
        int header = 40;     // top -> title centre
        int avail = std::max(win_h - 2 * margin, 1);
        int total = header + head_step + (btn_h + gap) * 4 + btn_h / 2;
        if (total > avail) {
            float s = static_cast<float>(avail) / static_cast<float>(total);
            header = std::max(1, static_cast<int>(header * s));
            head_step = std::max(1, static_cast<int>(head_step * s));
            btn_h = std::max(1, static_cast<int>(btn_h * s));
            gap = std::max(1, static_cast<int>(gap * s));
            total = header + head_step + (btn_h + gap) * 4 + btn_h / 2;
        }
        int top = std::max((win_h - total) / 2, margin);
        MenuLayout ml;
        ml.title_cy = top + header;
        int bw = 230;
        int cy = ml.title_cy + head_step;
        for (int i = 0; i < 5; ++i) {
            ml.buttons[i] = {win_w / 2 - bw / 2, cy - btn_h / 2, bw, btn_h};
            cy += btn_h + gap;
        }
        return ml;
    };

    auto fill_rect = [&](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        SDL_Rect rect{x, y, w, h};
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderFillRect(renderer, &rect);
    };

    auto stroke_rect = [&](int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_Rect rect{x, y, w, h};
        SDL_RenderDrawRect(renderer, &rect);
    };

    auto draw_checker = [&](int w, int h) {
        for (int cy = 0, y = 0; y < h; y += 64, ++cy) {
            for (int cx = 0, x = 0; x < w; x += 64, ++cx) {
                fill_rect(x, y, 64, 64, (cx + cy) % 2 == 0 ? 8 : 16,
                          (cx + cy) % 2 == 0 ? 8 : 16,
                          (cx + cy) % 2 == 0 ? 8 : 16);
            }
        }
    };

    auto draw_cell = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        fill_rect(offset_x + x * cell, offset_y + y * cell, cell, cell, r, g, b);
    };

    auto text_size = [&](TTF_Font* font, const char* text, int& w, int& h) {
        w = 0;
        h = 0;
        if (font) TTF_SizeUTF8(font, text, &w, &h);
    };

    auto render_text = [&](TTF_Font* font, const char* text, int center_x, int center_y,
                           uint8_t r, uint8_t g, uint8_t b, uint8_t alpha = 255) {
        if (!font) return;
        SDL_Color color{r, g, b, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        int w = surf->w;
        int h = surf->h;
        SDL_FreeSurface(surf);
        if (!tex) return;
        SDL_SetTextureAlphaMod(tex, alpha);
        SDL_Rect dst{center_x - w / 2, center_y - h / 2, w, h};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    };

    auto open_url = [&](const char* url) {
        log_event("OPEN url=%s", url);
#ifdef _WIN32
        std::string cmd = "start \"\" \"";
        cmd += url;
        cmd += "\"";
        std::system(cmd.c_str());
#else
        std::string cmd = "xdg-open \"";
        cmd += url;
        cmd += "\" >/dev/null 2>&1 &";
        std::system(cmd.c_str());
#endif
    };

    auto credit_link_rects = [&](int win_w, int win_h, SDL_Rect rects[CREDIT_LINK_COUNT]) {
        int cx = win_w / 2;
        int cy = win_h / 2;
        for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
            int wl = 0, hl = 0, wu = 0, hu = 0;
            text_size(font_small, CREDIT_LINKS[i].label, wl, hl);
            text_size(font_small, CREDIT_LINKS[i].url, wu, hu);
            int x0 = cx - (wl + wu) / 2;
            int y = cy + CREDIT_LINKS[i].dy - hu / 2;
            rects[i] = {x0 + wl, y, wu, hu};
        }
    };

    bool running = true;
    bool min_size_set = false;
    bool fullscreen = false;
    Uint32 last_tick = SDL_GetTicks();
    int best_score = 0;

    std::string scores_path;
    {
        char* p = SDL_GetPrefPath("KSnake", "KSnake");
        if (p) {
            scores_path = std::string(p) + "scores.json";
            SDL_free(p);
        } else {
            scores_path = "scores.json";
        }
    }

    auto load_best_score = [&]() {
        best_score = 0;
        std::ifstream f(scores_path);
        if (!f) return;
        json j;
        try {
            f >> j;
        } catch (...) {
            return;
        }
        auto it = j.find("sign");
        if (it == j.end() || !it->is_number_unsigned()) return;
        json payload = j;
        payload.erase("sign");
        if (fnv1a(payload.dump()) != it->get<uint32_t>()) return;
        if (j.contains("best_score") && j["best_score"].is_number()) {
            best_score = j["best_score"].get<int>();
        }
    };

    auto save_best_score = [&]() {
        json j;
        j["best_score"] = best_score;
        std::string body = j.dump();
        j["sign"] = fnv1a(body);
        std::ofstream f(scores_path);
        if (!f) return;
        f << j.dump(2) << std::endl;
    };

    {
        load_best_score();
        log_event("BEST score=%d path=%s", best_score, scores_path.c_str());
    }

    auto toggle_fullscreen = [&]() {
        if (SDL_SetWindowFullscreen(window,
                                    fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
            fullscreen = !fullscreen;
        }
    };

    auto key_blocked = [&](SDL_Scancode sc) {
        switch (sc) {
            case SDL_SCANCODE_F1:
            case SDL_SCANCODE_F2:
            case SDL_SCANCODE_F3:
            case SDL_SCANCODE_F4:
            case SDL_SCANCODE_F5:
            case SDL_SCANCODE_F6:
            case SDL_SCANCODE_F7:
            case SDL_SCANCODE_F8:
            case SDL_SCANCODE_F9:
            case SDL_SCANCODE_F10:
            case SDL_SCANCODE_F11:
            case SDL_SCANCODE_F12:
            case SDL_SCANCODE_F13:
            case SDL_SCANCODE_F14:
            case SDL_SCANCODE_F15:
            case SDL_SCANCODE_F16:
            case SDL_SCANCODE_F17:
            case SDL_SCANCODE_F18:
            case SDL_SCANCODE_F19:
            case SDL_SCANCODE_F20:
            case SDL_SCANCODE_F21:
            case SDL_SCANCODE_F22:
            case SDL_SCANCODE_F23:
            case SDL_SCANCODE_F24:
            case SDL_SCANCODE_LGUI:
            case SDL_SCANCODE_RGUI:
            case SDL_SCANCODE_LCTRL:
            case SDL_SCANCODE_RCTRL:
            case SDL_SCANCODE_LALT:
            case SDL_SCANCODE_RALT:
            case SDL_SCANCODE_LSHIFT:
            case SDL_SCANCODE_RSHIFT:
            case SDL_SCANCODE_TAB:
                return true;
            default:
                return false;
        }
    };

    auto activate = [&](int index) {
        if (index == 0) {
            reset_game();
            paused = false;
            game_over_active = false;
            screen = Screen::Game;
        } else if (index == 3) {
            screen = Screen::Credits;
        } else if (index == 4) {
            screen = Screen::ExitPrompt;
        } else {
            menu_coming[index] = true;
            menu_coming_start[index] = SDL_GetTicks();
            menu_coming_until[index] = menu_coming_start[index] + 1500;
        }
    };

    auto do_step = [&]() -> bool {
        if (!pending_turns.empty()) {
            dx = pending_turns.front().first;
            dy = pending_turns.front().second;
            pending_turns.pop_front();
        }
        Point head = snake.front();
        head.x += dx;
        head.y += dy;

        // Check wall collision before pushing to avoid rendering out-of-bounds
        if (head.x < 0 || head.x >= WIDTH || head.y < 0 || head.y >= HEIGHT) {
            log_event("GAMEOVER head=%d,%d cause=wall score=%d", head.x, head.y, score);
            paused = false;
            game_over_active = true;
            over_ms = SDL_GetTicks();
            if (score > best_score) {
                best_score = score;
                save_best_score();
                log_event("BEST new score=%d", best_score);
            }
            return false;
        }

        // Check self collision before pushing
        if (std::any_of(snake.begin(), snake.end(), [&](const Point& p) { return p == head; })) {
            log_event("GAMEOVER head=%d,%d cause=self score=%d", head.x, head.y, score);
            paused = false;
            game_over_active = true;
            over_ms = SDL_GetTicks();
            if (score > best_score) {
                best_score = score;
                save_best_score();
                log_event("BEST new score=%d", best_score);
            }
            return false;
        }

        snake.push_front(head);
        log_event("STEP x=%d y=%d len=%zu", head.x, head.y, snake.size());
        if (head == food) {
            ++score;
            move_interval = static_cast<Uint32>(std::max(60, 150 - score * 3));
            log_event("FOOD x=%d y=%d score=%d interval=%u",
                      food.x, food.y, score, move_interval);
            
            if (static_cast<int>(snake.size()) >= WIDTH * HEIGHT) {
                log_event("GAMEWON score=%d", score);
                paused = false;
                game_won_active = true;
                over_ms = SDL_GetTicks();
                if (score > best_score) {
                    best_score = score;
                    save_best_score();
                }
                return false;
            }
            place_food();
        } else {
            snake.pop_back();
        }
        return true;
    };

    while (running) {
        Uint32 now = SDL_GetTicks();
        Uint32 frame_delta = now - last_tick;
        last_tick = now;

        // Prevent "spiral of death" (extreme speedups) after freezes
        if (frame_delta > 250) {
            frame_delta = 250;
        }

        if (!min_size_set) {
            const char* vid_driver = SDL_GetCurrentVideoDriver();
            bool wayland = vid_driver && (strcmp(vid_driver, "wayland") == 0);
            if (!wayland) {
                SDL_SetWindowMinimumSize(window, MIN_WIN_W, MIN_WIN_H);
            }
            min_size_set = true;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    compute_layout(event.window.data1, event.window.data2);
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                           event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    if (screen == Screen::Game && !game_over_active && !game_won_active) {
                        paused = true;
                        log_event("PAUSE focus lost");
                    }
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                if (screen == Screen::Menu) {
                    int mw = 0, mh = 0;
                    SDL_GetWindowSize(window, &mw, &mh);
                    SDL_Point pt{event.motion.x, event.motion.y};
                    MenuLayout ml = menu_layout(mw, mh);
                    for (int i = 0; i < 5; ++i) {
                        if (SDL_PointInRect(&pt, &ml.buttons[i])) {
                            menu_index = i;
                            break;
                        }
                    }
                } else if (screen == Screen::Credits) {
                    int mw = 0, mh = 0;
                    SDL_GetWindowSize(window, &mw, &mh);
                    SDL_Point pt{event.motion.x, event.motion.y};
                    SDL_Rect link_rects[CREDIT_LINK_COUNT];
                    credit_link_rects(mw, mh, link_rects);
                    credit_hover = -1;
                    for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
                        if (SDL_PointInRect(&pt, &link_rects[i])) {
                            credit_hover = i;
                            break;
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    int mw = 0, mh = 0;
                    SDL_GetWindowSize(window, &mw, &mh);
                    SDL_Point pt{event.button.x, event.button.y};
                    if (screen == Screen::Menu) {
                        MenuLayout ml = menu_layout(mw, mh);
                        for (int i = 0; i < 5; ++i) {
                            if (SDL_PointInRect(&pt, &ml.buttons[i])) {
                                menu_index = i;
                                activate(i);
                                break;
                            }
                        }
                    } else if (screen == Screen::Credits) {
                        SDL_Rect link_rects[CREDIT_LINK_COUNT];
                        credit_link_rects(mw, mh, link_rects);
                        for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
                            if (SDL_PointInRect(&pt, &link_rects[i])) {
                                open_url(CREDIT_LINKS[i].url);
                                break;
                            }
                        }
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.repeat) continue;
                SDL_Scancode sc = event.key.keysym.scancode;

                if (sc == SDL_SCANCODE_F11) {
                    toggle_fullscreen();
                    continue;
                }

                if (screen == Screen::Menu) {
                    switch (sc) {
                        case SDL_SCANCODE_UP:
                            menu_index = (menu_index + 4) % 5;
                            break;
                        case SDL_SCANCODE_DOWN:
                            menu_index = (menu_index + 1) % 5;
                            break;
                        case SDL_SCANCODE_RETURN:
                        case SDL_SCANCODE_SPACE:
                            activate(menu_index);
                            break;
                        case SDL_SCANCODE_P:
                        case SDL_SCANCODE_1:
                            activate(0);
                            break;
                        case SDL_SCANCODE_2:
                            activate(1);
                            break;
                        case SDL_SCANCODE_3:
                            activate(2);
                            break;
                        case SDL_SCANCODE_4:
                            activate(3);
                            break;
                        case SDL_SCANCODE_5:
                            activate(4);
                            break;
                        case SDL_SCANCODE_ESCAPE:
                            screen = Screen::ExitPrompt;
                            break;
                        default:
                            break;
                    }
                } else if (screen == Screen::ExitPrompt) {
                    if (sc == SDL_SCANCODE_RETURN) {
                        running = false;
                    } else if (!key_blocked(sc)) {
                        screen = Screen::Menu;
                    }
                } else if (screen == Screen::Credits) {
                    if (!key_blocked(sc)) {
                        screen = Screen::Menu;
                        credit_hover = -1;
                    }
                } else {
                    if (game_over_active || game_won_active) {
                        if (sc == SDL_SCANCODE_R) {
                            reset_game();
                            paused = false;
                        } else if (!key_blocked(sc) && SDL_GetTicks() >= over_ms + 1000) {
                            game_over_active = false;
                            game_won_active = false;
                            paused = false;
                            screen = Screen::Menu;
                            menu_index = 0;
                        }
                    } else if (paused) {
                        if (sc == SDL_SCANCODE_P || sc == SDL_SCANCODE_SPACE) {
                            paused = false;
                            log_event("PAUSE off");
                        } else if (sc == SDL_SCANCODE_ESCAPE) {
                            paused = false;
                            screen = Screen::Menu;
                            menu_index = 0;
                            log_event("PAUSE to menu");
                        }
                    } else {
                        switch (sc) {
                            case SDL_SCANCODE_UP: queue_turn(0, -1); break;
                            case SDL_SCANCODE_DOWN: queue_turn(0, 1); break;
                            case SDL_SCANCODE_LEFT: queue_turn(-1, 0); break;
                            case SDL_SCANCODE_RIGHT: queue_turn(1, 0); break;
                            case SDL_SCANCODE_P:
                                paused = !paused;
                                log_event("PAUSE %s", paused ? "on" : "off");
                                break;
                            case SDL_SCANCODE_ESCAPE:
                                screen = Screen::Menu;
                                menu_index = 0;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }

        if (screen == Screen::Game && !paused && !game_over_active && !game_won_active) {
            igt_ms += frame_delta;
            move_acc += frame_delta;
            while (move_acc >= move_interval && !game_over_active && !game_won_active) {
                move_acc -= move_interval;
                if (!do_step()) break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(window, &win_w, &win_h);

        if (screen == Screen::Game) {
            // Draw border around playing field
            stroke_rect(offset_x - 1, offset_y - 1, WIDTH * cell + 2, HEIGHT * cell + 2, 70, 70, 70);

            for (int gy = 0; gy < HEIGHT; ++gy) {
                for (int gx = 0; gx < WIDTH; ++gx) {
                    draw_cell(gx, gy, (gx + gy) % 2 == 0 ? 0 : 24,
                              (gx + gy) % 2 == 0 ? 0 : 24,
                              (gx + gy) % 2 == 0 ? 0 : 24);
                }
            }

            draw_cell(food.x, food.y, 231, 76, 60);
            for (size_t i = 0; i < snake.size(); ++i) {
                if (i == 0) {
                    // Head: brighter green
                    draw_cell(snake[i].x, snake[i].y, 46, 204, 113);
                    // Draw eyes on head
                    int eye_sz = std::max(2, cell / 7);
                    int hx = offset_x + snake[i].x * cell;
                    int hy = offset_y + snake[i].y * cell;
                    if (dx == 1) {
                        fill_rect(hx + cell - eye_sz * 2, hy + eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                        fill_rect(hx + cell - eye_sz * 2, hy + cell - eye_sz * 3, eye_sz, eye_sz, 20, 20, 20);
                    } else if (dx == -1) {
                        fill_rect(hx + eye_sz, hy + eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                        fill_rect(hx + eye_sz, hy + cell - eye_sz * 3, eye_sz, eye_sz, 20, 20, 20);
                    } else if (dy == 1) {
                        fill_rect(hx + eye_sz * 2, hy + cell - eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                        fill_rect(hx + cell - eye_sz * 3, hy + cell - eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                    } else if (dy == -1) {
                        fill_rect(hx + eye_sz * 2, hy + eye_sz, eye_sz, eye_sz, 20, 20, 20);
                        fill_rect(hx + cell - eye_sz * 3, hy + eye_sz, eye_sz, eye_sz, 20, 20, 20);
                    }
                } else {
                    // Body: slightly darker green
                    draw_cell(snake[i].x, snake[i].y, 39, 174, 96);
                }
            }

            if (paused) {
                SDL_Rect full{0, 0, win_w, win_h};
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 170);
                SDL_RenderFillRect(renderer, &full);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
                render_text(font_title, "Paused", win_w / 2, win_h / 2, 255, 255, 255);
                render_text(font_small, "Press P or Space to continue  |  Esc for Menu",
                            win_w / 2, win_h / 2 + 50, 180, 180, 180);
            } else if (game_over_active || game_won_active) {
                int cx = win_w / 2;
                int cy = win_h / 2;
                SDL_Rect full{0, 0, win_w, win_h};
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
                SDL_RenderFillRect(renderer, &full);
                SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

                int pw = std::min(420, win_w - 16);
                int ph = std::min(230, win_h - 16);
                fill_rect(cx - pw / 2, cy - ph / 2, pw, ph, 26, 26, 26);
                stroke_rect(cx - pw / 2, cy - ph / 2, pw, ph, 70, 70, 70);

                int pad = 20;
                int panel_top = cy - ph / 2;
                int panel_bot = cy + ph / 2;

                const char* title_text = game_won_active ? "You Won!" : "Game Over!";
                int tw = 0, th = 0, sh = 0, hh = 0, dd = 0;
                text_size(font_gameover, title_text, tw, th);
                text_size(font_small, "New Score: 0", dd, sh);
                text_size(font_small, "Press R to restart  |  Any key to continue", dd, hh);

                int title_cy = panel_top + pad + th / 2;
                int hint_cy = panel_bot - pad - hh / 2;

                int gap = 12;
                int group_top = title_cy + th / 2 + gap;
                int group_bot = hint_cy - hh / 2 - gap;

                int line_step = sh + 4;
                if (group_bot - group_top > line_step * 2) {
                    int mid = (group_top + group_bot) / 2;
                    group_top = mid - line_step;
                    group_bot = mid + line_step;
                }

                int score_cy = group_top;
                int time_cy  = group_top + line_step;
                int best_cy  = group_top + 2 * line_step;

                if (game_won_active) {
                    render_text(font_gameover, title_text, cx, title_cy, 46, 204, 113);
                } else {
                    render_text(font_gameover, title_text, cx, title_cy, 231, 76, 60);
                }
                render_text(font_small, ("New Score: " + std::to_string(score)).c_str(),
                            cx, score_cy, 255, 255, 255);
                int tm = static_cast<int>(igt_ms / 1000);
                render_text(font_small, ("Time: " + std::to_string(tm / 60) + ":" +
                                          (tm % 60 < 10 ? "0" : "") + std::to_string(tm % 60)).c_str(),
                            cx, time_cy, 255, 255, 255);
                render_text(font_small, ("Best: " + std::to_string(best_score)).c_str(),
                            cx, best_cy, 255, 255, 255);
                const char* hint_text = "Press R to restart | Any key to continue";
                int el = static_cast<int>(SDL_GetTicks()) - static_cast<int>(over_ms);
                if (el < 0) el = 0;
                float fade = static_cast<float>(el) / 1000.0f;
                if (fade > 1.0f) fade = 1.0f;
                render_text(font_small, hint_text, cx, hint_cy, 180, 180, 180,
                            static_cast<uint8_t>(255.0f * fade));
            }
        } else if (screen == Screen::ExitPrompt) {

            SDL_Rect full{0, 0, win_w, win_h};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
            SDL_RenderFillRect(renderer, &full);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            int cx = win_w / 2;
            int cy = win_h / 2;
            int ew = std::min(480, win_w - 16);
            int eh = std::min(160, win_h - 16);
            fill_rect(cx - ew / 2, cy - eh / 2, ew, eh, 26, 26, 26);
            stroke_rect(cx - ew / 2, cy - eh / 2, ew, eh, 70, 70, 70);
            int w1 = 0, w2 = 0, h1 = 0, h2 = 0;
            text_size(font_button, "Do you want to ", w1, h1);
            text_size(font_button, "exit?", w2, h2);
            int start = cx - (w1 + w2) / 2;
            int line_cy = cy - 30;
            render_text(font_button, "Do you want to ", start + w1 / 2, line_cy, 255, 255, 255);
            render_text(font_button, "exit?", start + w1 + w2 / 2, line_cy, 231, 76, 60);
            render_text(font_small, "Enter to exit  |  Esc to cancel",
                        cx, cy + 38, 180, 180, 180);
        } else if (screen == Screen::Credits) {
            draw_checker(win_w, win_h);

            SDL_Rect full{0, 0, win_w, win_h};
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
            SDL_RenderFillRect(renderer, &full);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

            int cx = win_w / 2;
            int cy = win_h / 2;
            int cw = std::min(520, win_w - 16);
            int ch = std::min(280, win_h - 16);
            fill_rect(cx - cw / 2, cy - ch / 2, cw, ch, 26, 26, 26);
            stroke_rect(cx - cw / 2, cy - ch / 2, cw, ch, 70, 70, 70);

            render_text(font_title, "Credits", cx, cy - 95, 255, 255, 255);
            SDL_Rect link_rects[CREDIT_LINK_COUNT];
            credit_link_rects(win_w, win_h, link_rects);
            for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
                int wl = 0, hl = 0, wu = 0, hu = 0;
                text_size(font_small, CREDIT_LINKS[i].label, wl, hl);
                text_size(font_small, CREDIT_LINKS[i].url, wu, hu);
                int x0 = link_rects[i].x - wl;
                int ly = link_rects[i].y + link_rects[i].h / 2;
                int yu = link_rects[i].y;
                bool hover = (i == credit_hover);
                render_text(font_small, CREDIT_LINKS[i].label,
                            x0 + wl / 2, ly, 140, 140, 140);
                if (hover) {
                    render_text(font_small, CREDIT_LINKS[i].url,
                                link_rects[i].x + wu / 2, ly, 232, 232, 232);
                    fill_rect(link_rects[i].x, yu + hu + 2,
                              wu, 2, 232, 232, 232);
                } else {
                    render_text(font_small, CREDIT_LINKS[i].url,
                                link_rects[i].x + wu / 2, ly, 180, 180, 180);
                }
            }
            render_text(font_small, "Powered by SDL2 & SDL2_ttf (C++)",
                        cx, cy + 75, 150, 150, 150);
            render_text(font_small, "Any key to return",
                        cx, cy + 105, 255, 255, 255);
        } else {
            draw_checker(win_w, win_h);

            MenuLayout ml = menu_layout(win_w, win_h);
            int btn_h = ml.buttons[0].h;
            int title_cy = ml.title_cy;
            int btn1_cy = ml.buttons[0].y + ml.buttons[0].h / 2;
            int btn2_cy = ml.buttons[1].y + ml.buttons[1].h / 2;
            int btn3_cy = ml.buttons[2].y + ml.buttons[2].h / 2;
            int btn4_cy = ml.buttons[3].y + ml.buttons[3].h / 2;
            int btn5_cy = ml.buttons[4].y + ml.buttons[4].h / 2;

            render_text(font_title, "KSnake", win_w / 2, title_cy, 255, 255, 255);

            auto draw_button = [&](const char* label, int center_y, int index) {
                int bw = 230;
                int bh = btn_h;
                bool active = (index == menu_index);
                bool is_coming = menu_coming[index] && now < menu_coming_until[index];

                float progress = 0.0f;
                if (is_coming) {
                    progress = static_cast<float>(now - menu_coming_start[index]) / 1500.0f;
                    if (progress < 0.0f) progress = 0.0f;
                    if (progress > 1.0f) progress = 1.0f;
                }

                int x0 = win_w / 2 - bw / 2;
                int y0 = center_y - bh / 2;

                uint8_t bg = active ? 66 : 30;

                if (index == 0) {
                    uint8_t bg_r = active ? 56 : 20;
                    uint8_t bg_g = active ? 110 : 40;
                    uint8_t bg_b = active ? 62 : 24;
                    uint8_t tx_r = active ? 150 : 46;
                    uint8_t tx_g = active ? 255 : 204;
                    uint8_t tx_b = active ? 175 : 113;
                    fill_rect(x0, y0, bw, bh, bg_r, bg_g, bg_b);
                    if (active) stroke_rect(x0, y0, bw, bh, tx_r, tx_g, tx_b);
                    render_text(font_button, label, win_w / 2, center_y, tx_r, tx_g, tx_b);
                } else if (index == 4) {
                    uint8_t bg_r = active ? 130 : 46;
                    uint8_t bg_g = active ? 40 : 22;
                    uint8_t bg_b = active ? 40 : 22;
                    uint8_t tx_r = active ? 255 : 231;
                    uint8_t tx_g = active ? 120 : 76;
                    uint8_t tx_b = active ? 120 : 60;
                    fill_rect(x0, y0, bw, bh, bg_r, bg_g, bg_b);
                    if (active) stroke_rect(x0, y0, bw, bh, tx_r, tx_g, tx_b);
                    render_text(font_button, label, win_w / 2, center_y, tx_r, tx_g, tx_b);
                } else {
                    fill_rect(x0, y0, bw, bh, bg, bg, bg);
                    if (active) stroke_rect(x0, y0, bw, bh, 190, 190, 190);
                    if (is_coming) {
                        render_text(font_button, "Coming soon..", win_w / 2, center_y,
                                    200, 205, 200, static_cast<uint8_t>(255.0f * (1.0f - progress)));
                        render_text(font_button, label, win_w / 2, center_y,
                                    active ? 190 : 130, active ? 190 : 130, active ? 190 : 130,
                                    static_cast<uint8_t>(255.0f * progress));
                    } else {
                        render_text(font_button, label, win_w / 2, center_y,
                                    active ? 190 : 130, active ? 190 : 130, active ? 190 : 130);
                    }
                }
            };

            draw_button("Play", btn1_cy, 0);
            draw_button("Skins", btn2_cy, 1);
            draw_button("Settings", btn3_cy, 2);
            draw_button("Credits", btn4_cy, 3);
            draw_button("Exit", btn5_cy, 4);

            for (int i = 1; i < 3; ++i) {
                if (menu_coming[i] && now >= menu_coming_until[i]) {
                    menu_coming[i] = false;
                }
            }
        }

        SDL_RenderPresent(renderer);

        if (frame_delta < 1000 / FPS) {
            SDL_Delay((1000 / FPS) - frame_delta);
        }
    }

    if (font_title) TTF_CloseFont(font_title);
    if (font_button) TTF_CloseFont(font_button);
    if (font_small) TTF_CloseFont(font_small);
    if (font_gameover) TTF_CloseFont(font_gameover);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    log_event("END score=%d screen=%d", score, static_cast<int>(screen));
    if (log_fp) std::fclose(log_fp);
    return 0;
}