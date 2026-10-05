#ifdef _WIN32
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#include <SDL_ttf.h>

#ifdef _WIN32
// ShellExecuteW для открытия ссылки без оболочки (см. open_url).
// shellapi.h требует windows.h первым.
#include <windows.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <functional>
#include <random>
#include <string>

#include "config.h"
#include "game.h"
#include "screens.h"
#include "storage.h"
#include "ui.h"
#include "update.h"
#include "util.h"
#include "version.h"

// Точка сборки: здесь только инициализация, ввод и главный цикл. Логика игры в
// game.cpp, отрисовка в ui.cpp/screens.cpp, проверка обновлений в update.cpp.
namespace ks {
namespace {

void open_url(const std::string& url, const std::function<void(const std::string&)>& log) {
    log("OPEN url=" + url);
#ifdef _WIN32
    // ShellExecute без оболочки: кавычки и & в URL не превращаются в команду.
    int rc = static_cast<int>(reinterpret_cast<intptr_t>(
        ShellExecuteW(nullptr, L"open", utf8_to_wide(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL)));
    log(rc > 32 ? "OPEN ok" : "OPEN failed");
#else
    spawn_detached({"xdg-open", url});
#endif
}

}  // namespace

int run(int argc, char* argv[]) {
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
    auto log_line = [log_event](const std::string& msg) {
        log_event("%s", msg.c_str());
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
        SDL_CreateWindow("KSnake", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIN_W, WIN_H,
                         SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        if (log_fp) std::fclose(log_fp);
        return -1;
    }
    // SDL_SetWindowMinimumSize is applied on the first frame.
    // On the Wayland backend applying it at creation shrinks the window
    // down to the minimum size (SDL behavior), so Wayland keeps no
    // hard minimum; the layout is clamped internally instead.

    int init_w = 0, init_h = 0;
    SDL_GetWindowSize(window, &init_w, &init_h);
    log_event("WIN w=%d h=%d", init_w, init_h);

    const char* version_source = "fallback";
    std::string app_version = load_app_version(&version_source);
    {
        int parsed[3] = {0, 0, 0};
        if (!parse_version(app_version, parsed)) {
            log_event("VERSION bad value, using fallback");
            app_version = "1.0";
            version_source = "fallback";
        } else {
            log_event("VERSION %s from %s", app_version.c_str(), version_source);
        }
    }

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
        if (log_fp) std::fclose(log_fp);
        return -1;
    }

    SDL_RendererInfo rinfo;
    if (SDL_GetRendererInfo(renderer, &rinfo) == 0) {
        log_event("RENDERER driver=%s", rinfo.name);
    }

    const uint32_t seed = static_cast<uint32_t>(std::time(nullptr)) ^
                          static_cast<uint32_t>(SDL_GetTicks()) ^ std::random_device{}();

    Ui ui(renderer, seed);
    ui.set_logger(log_line);
    if (!ui.load_fonts()) {
        fprintf(stderr, "No usable font found. Tried:\n");
        for (const char* path : FONT_CANDIDATES) {
            fprintf(stderr, "  %s\n", path);
        }
        log_event("FATAL no usable font found");
        ui.destroy();
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        if (log_fp) std::fclose(log_fp);
        return -1;
    }

    Game game(seed);
    ScreenState screen_state;

    const std::string pref_dir = user_pref_dir();
    const std::string scores_path = pref_dir + "scores.json";
    int best_score = load_best_score(scores_path);
    log_event("BEST score=%d path=%s", best_score, scores_path.c_str());

    UpdateChecker updates(pref_dir, app_version);
    updates.set_logger(log_line);
    updates.restore_cached();
    updates.start_background_cycle(SDL_GetTicks64());

    bool running = true;
    bool min_size_set = false;
    bool fullscreen = false;
    Uint32 last_tick = SDL_GetTicks();
    bool paused = false;
    bool game_over_active = false;
    bool game_won_active = false;
    Uint32 igt_ms = 0;      // in-game time, excludes paused/game-over time
    Uint32 move_acc = 0;

    auto apply_window_layout = [&]() {
        int w = WIN_W, h = WIN_H;
        SDL_GetWindowSize(window, &w, &h);
        ui.apply_layout(w, h);
    };

    apply_window_layout();
    ui.shuffle_checker();

    auto toggle_fullscreen = [&]() {
        if (SDL_SetWindowFullscreen(window, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP) == 0) {
            fullscreen = !fullscreen;
        }
    };

    auto key_blocked = [&](SDL_Scancode sc) {
        return (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) ||
               (sc >= SDL_SCANCODE_F13 && sc <= SDL_SCANCODE_F24) ||
               sc == SDL_SCANCODE_LGUI || sc == SDL_SCANCODE_RGUI ||
               sc == SDL_SCANCODE_LCTRL || sc == SDL_SCANCODE_RCTRL ||
               sc == SDL_SCANCODE_LALT || sc == SDL_SCANCODE_RALT ||
               sc == SDL_SCANCODE_LSHIFT || sc == SDL_SCANCODE_RSHIFT ||
               sc == SDL_SCANCODE_TAB;
    };

    auto finish_game = [&](bool won) {
        paused = false;
        game_over_active = !won;
        game_won_active = won;
        if (game.score() > best_score) {
            best_score = game.score();
            save_best_score(scores_path, best_score);
            log_event("BEST new score=%d", best_score);
        }
    };

    auto step_game = [&]() -> bool {
        StepOutcome out = game.step();
        switch (out.kind) {
            case StepKind::Moved:
            case StepKind::AteFood:
                log_event("STEP x=%d y=%d len=%zu", out.head.x, out.head.y,
                          game.snake().size());
                if (out.kind == StepKind::AteFood) {
                    log_event("FOOD x=%d y=%d score=%d interval=%u", game.food().x,
                              game.food().y, out.score, out.move_interval);
                }
                return true;
            case StepKind::Won:
                log_event("GAMEWON score=%d", out.score);
                finish_game(true);
                return false;
            case StepKind::WallCollision:
                log_event("GAMEOVER head=%d,%d cause=wall score=%d", out.head.x, out.head.y,
                          out.score);
                finish_game(false);
                return false;
            case StepKind::SelfCollision:
                log_event("GAMEOVER head=%d,%d cause=self score=%d", out.head.x, out.head.y,
                          out.score);
                finish_game(false);
                return false;
        }
        return true;
    };

    auto game_to_menu = [&]() {
        paused = false;
        game_over_active = false;
        game_won_active = false;
        screen_state.screen = Screen::Menu;
        screen_state.menu_index = MENU_PLAY;
        screen_state.hint_hover = -1;
        screen_state.overlay_hover = -1;
        screen_state.overlay_button_count = 0;
        ui.shuffle_checker();  // новый случайный сдвиг фона при возврате в меню
    };

    auto screen_to_menu = [&]() {
        screen_state.hint_hover = -1;
        screen_state.screen = Screen::Menu;
    };

    auto run_hint = [&](HintAction action) {
        screen_state.hint_hover = -1;
        switch (action) {
            case HintAction::Restart:
                // с Game Over "Restart" обязан снимать и оверлей, иначе поле
                // остаётся застывшим: шаги не идут, пока активен game_over_active
                game.reset();
                paused = false;
                game_over_active = false;
                game_won_active = false;
                igt_ms = 0;
                move_acc = 0;
                screen_state.screen = Screen::Game;
                screen_state.overlay_hover = -1;
                screen_state.overlay_button_count = 0;
                log_event("RESTART score=%d", game.score());
                break;
            case HintAction::Continue:
                if (game_over_active || game_won_active) {
                    game_to_menu();
                } else {
                    paused = false;
                }
                break;
            case HintAction::Menu:
                game_to_menu();
                break;
            case HintAction::Confirm:
                running = false;
                break;
            case HintAction::Cancel:
                screen_state.screen = Screen::Menu;
                break;
            default:
                break;
        }
    };

    auto activate = [&](int index) {
        if (index < 0 || index >= MENU_COUNT) return;
        if (index == MENU_PLAY) {
            game.reset();
            paused = false;
            game_over_active = false;
            screen_state.screen = Screen::Game;
        } else if (index == MENU_CREDITS) {
            screen_state.screen = Screen::Credits;
        } else if (index == MENU_UPDATES) {
            screen_state.screen = Screen::Update;
            if (updates.result_is_fresh(SDL_GetTicks64())) {
                log_event("UPDATE show cached state=%d", static_cast<int>(updates.state()));
            } else {
                updates.start(false);
            }
        } else if (index == MENU_EXIT) {
            screen_state.screen = Screen::ExitPrompt;
        } else {
            screen_state.menu_coming[index] = true;
            screen_state.menu_coming_start[index] = SDL_GetTicks();
            screen_state.menu_coming_until[index] =
                screen_state.menu_coming_start[index] + 1500;
        }
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
            // Минимум ставим на первом кадре, а не при создании окна: на Wayland
            // применение минимума в момент создания схлопывало окно до него
            // самого. На первом кадре этого не происходит, зато минимум работает
            // на всех бэкендах. Ниже 400x360 вёрстка не помещается: Credits и
            // экраны Updates требуют этого размера, поэтому меньше не даём.
            SDL_SetWindowMinimumSize(window, MIN_WIN_W, MIN_WIN_H);
            int mw = 0, mh = 0;
            SDL_GetWindowMinimumSize(window, &mw, &mh);
            log_event("WIN minimum=%dx%d", mw, mh);
            min_size_set = true;
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    apply_window_layout();
                } else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
                           event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    if (screen_state.screen == Screen::Game && !game_over_active &&
                        !game_won_active) {
                        paused = true;
                        log_event("PAUSE focus lost");
                    }
                }
            } else if (event.type == SDL_MOUSEMOTION) {
                SDL_Point pt{event.motion.x, event.motion.y};
                screen_state.hint_hover = hint_at(screen_state, &pt, screen_state.screen);
                screen_state.overlay_hover =
                    overlay_at(screen_state, &pt, screen_state.screen, paused, game_over_active,
                               game_won_active);
                if (screen_state.screen == Screen::Menu) {
                    MenuLayout ml = menu_layout(ui, ui.lay_w(), ui.lay_h());
                    for (int i = 0; i < MENU_COUNT; ++i) {
                        if (SDL_PointInRect(&pt, &ml.buttons[i])) {
                            screen_state.menu_index = i;
                            break;
                        }
                    }
                } else if (screen_state.screen == Screen::Update) {
                    screen_state.update_link_hover =
                        screen_state.update_link_hit.w > 0 &&
                                SDL_PointInRect(&pt, &screen_state.update_link_hit)
                            ? 0
                            : -1;
                } else if (screen_state.screen == Screen::Credits) {
                    SDL_Rect link_rects[CREDIT_LINK_COUNT];
                    credit_link_rects(ui, ui.lay_w(), ui.lay_h(), link_rects);
                    screen_state.credit_hover = -1;
                    for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
                        if (SDL_PointInRect(&pt, &link_rects[i])) {
                            screen_state.credit_hover = i;
                            break;
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                SDL_Point pt{event.button.x, event.button.y};
                if (screen_state.screen == Screen::Game) {
                    int oidx = overlay_at(screen_state, &pt, screen_state.screen, paused,
                                          game_over_active, game_won_active);
                    if (oidx >= 0) {
                        run_hint(screen_state.overlay_buttons[oidx].action);
                        screen_state.overlay_hover = -1;
                    } else {
                        int idx = hint_at(screen_state, &pt, screen_state.screen);
                        if (idx >= 0) run_hint(screen_state.hint_actions[idx]);
                    }
                } else if (event.button.button == SDL_BUTTON_LEFT) {
                    if (screen_state.screen == Screen::Menu) {
                        MenuLayout ml = menu_layout(ui, ui.lay_w(), ui.lay_h());
                        for (int i = 0; i < MENU_COUNT; ++i) {
                            if (SDL_PointInRect(&pt, &ml.buttons[i])) {
                                screen_state.menu_index = i;
                                activate(i);
                                break;
                            }
                        }
                    } else if (screen_state.screen == Screen::Credits) {
                        SDL_Rect link_rects[CREDIT_LINK_COUNT];
                        credit_link_rects(ui, ui.lay_w(), ui.lay_h(), link_rects);
                        bool opened = false;
                        for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
                            if (SDL_PointInRect(&pt, &link_rects[i])) {
                                open_url(CREDIT_LINKS[i].url, log_line);
                                opened = true;
                                break;
                            }
                        }
                        if (!opened) {
                            screen_to_menu();
                        }
                    } else if (screen_state.screen == Screen::Update) {
                        if (screen_state.update_link_hit.w > 0 &&
                            SDL_PointInRect(&pt, &screen_state.update_link_hit) &&
                            !updates.url().empty()) {
                            open_url(updates.url(), log_line);
                        } else {
                            screen_state.update_link_hover = -1;
                            screen_to_menu();
                        }
                    } else {
                        int idx = hint_at(screen_state, &pt, screen_state.screen);
                        if (idx >= 0) run_hint(screen_state.hint_actions[idx]);
                    }
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.repeat) continue;
                SDL_Scancode sc = event.key.keysym.scancode;

                if (sc == SDL_SCANCODE_F11) {
                    toggle_fullscreen();
                    continue;
                }

                if (screen_state.screen == Screen::Menu) {
                    switch (sc) {
                        case SDL_SCANCODE_UP:
                            screen_state.menu_index =
                                (screen_state.menu_index + MENU_COUNT - 1) % MENU_COUNT;
                            break;
                        case SDL_SCANCODE_DOWN:
                            screen_state.menu_index = (screen_state.menu_index + 1) % MENU_COUNT;
                            break;
                        case SDL_SCANCODE_RETURN:
                        case SDL_SCANCODE_SPACE:
                            activate(screen_state.menu_index);
                            break;
                        case SDL_SCANCODE_P:
                            activate(MENU_PLAY);
                            break;
                        case SDL_SCANCODE_1:
                        case SDL_SCANCODE_2:
                        case SDL_SCANCODE_3:
                        case SDL_SCANCODE_4:
                        case SDL_SCANCODE_5:
                        case SDL_SCANCODE_6:
                            activate(sc - SDL_SCANCODE_1);
                            break;
                        case SDL_SCANCODE_ESCAPE:
                            screen_state.screen = Screen::ExitPrompt;
                            break;
                        default:
                            break;
                    }
                } else if (screen_state.screen == Screen::ExitPrompt) {
                    if (sc == SDL_SCANCODE_RETURN) {
                        run_hint(HintAction::Confirm);
                    } else if (!key_blocked(sc)) {
                        run_hint(HintAction::Cancel);
                    }
                } else if (screen_state.screen == Screen::Credits) {
                    if (!key_blocked(sc)) {
                        screen_state.credit_hover = -1;
                        log_event("CREDITS key sc=%d to menu", static_cast<int>(sc));
                        screen_to_menu();
                    }
                } else if (screen_state.screen == Screen::Update) {
                    if (!key_blocked(sc)) {
                        screen_state.update_link_hover = -1;
                        screen_to_menu();
                    }
                } else {
                    if (game_over_active || game_won_active) {
                        if (sc == SDL_SCANCODE_SPACE || sc == SDL_SCANCODE_R) {
                            run_hint(HintAction::Restart);
                        } else if (sc == SDL_SCANCODE_ESCAPE) {
                            run_hint(HintAction::Menu);
                        }
                    } else if (paused) {
                        if (sc == SDL_SCANCODE_SPACE) {
                            run_hint(HintAction::Continue);
                            log_event("PAUSE off");
                        } else if (sc == SDL_SCANCODE_ESCAPE) {
                            game_to_menu();
                            log_event("PAUSE to menu");
                        }
                    } else {
                        switch (sc) {
                            case SDL_SCANCODE_UP: game.queue_turn(0, -1); break;
                            case SDL_SCANCODE_DOWN: game.queue_turn(0, 1); break;
                            case SDL_SCANCODE_LEFT: game.queue_turn(-1, 0); break;
                            case SDL_SCANCODE_RIGHT: game.queue_turn(1, 0); break;
                            case SDL_SCANCODE_ESCAPE:
                                paused = true;
                                log_event("PAUSE on");
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
        }

        updates.poll();
        updates.animate_glow(frame_delta);
        if (screen_state.screen != Screen::Update && updates.state() != UpdateState::Checking &&
            updates.background_due(SDL_GetTicks64())) {
            updates.start(true);
            updates.mark_checked();
            updates.schedule_next_background(SDL_GetTicks64());
            log_event("UPDATE background check again");
        }

        if (screen_state.screen == Screen::Game && !paused && !game_over_active &&
            !game_won_active) {
            igt_ms += frame_delta;
            move_acc += frame_delta;
            while (move_acc >= game.move_interval() && !game_over_active && !game_won_active) {
                move_acc -= game.move_interval();
                if (!step_game()) break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        ScreenFrame frame;
        frame.game = &game;
        frame.updates = &updates;
        frame.app_version = &app_version;
        frame.score = game.score();
        frame.igt_ms = igt_ms;
        frame.best_score = best_score;
        frame.now = now;
        frame.paused = paused;
        frame.game_over = game_over_active;
        frame.won = game_won_active;
        render_screen(ui, screen_state, frame);

        SDL_RenderPresent(renderer);

        if (frame_delta < 1000 / FPS) {
            SDL_Delay((1000 / FPS) - frame_delta);
        }
    }

    ui.destroy();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    log_event("END score=%d screen=%d", game.score(), static_cast<int>(screen_state.screen));
    if (log_fp) std::fclose(log_fp);
    return 0;
}

}  // namespace ks

int main(int argc, char* argv[]) {
    return ks::run(argc, argv);
}