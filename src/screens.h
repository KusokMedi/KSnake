#pragma once

#include <SDL.h>

#include <string>

#include "game.h"
#include "ui.h"
#include "update.h"

namespace ks {

enum class Screen { Menu, Game, ExitPrompt, Credits, Update };

enum class HintAction { None, Restart, Continue, Menu, Confirm, Cancel };

struct HintPart {
    const char* text;
    HintAction action;
};

constexpr int MAX_HINTS = 4;
constexpr const char* HINT_SEPARATOR = "  |  ";

constexpr int MENU_PLAY = 0;
constexpr int MENU_SKINS = 1;
constexpr int MENU_SETTINGS = 2;
constexpr int MENU_CREDITS = 3;
constexpr int MENU_UPDATES = 4;
constexpr int MENU_EXIT = 5;
constexpr int MENU_COUNT = 6;

struct MenuItem {
    const char* label;
    bool placeholder;
};

extern const MenuItem MENU_ITEMS[MENU_COUNT];

struct CreditLink {
    const char* label;
    const char* url;
    int dy;  // offset from window centre
};

extern const CreditLink CREDIT_LINKS[];
constexpr int CREDIT_LINK_COUNT = 4;

// Кнопки оверлеев паузы и Game Over: те же действия, что и на клавиатуре.
struct OverlayButton {
    const char* label;
    HintAction action;
    SDL_Rect rect;
    bool danger;  // красная кнопка, как Exit в меню; иначе зелёная, как Play
};

// Всё, что считается при отрисовке и потом используется вводом: хит-тесты
// ховеров, прямоугольники подсказок и кнопок. Рендер их заполняет, обработчики
// событий — читают.
struct ScreenState {
    Screen screen = Screen::Menu;
    int menu_index = MENU_PLAY;
    bool menu_coming[MENU_COUNT] = {};
    Uint32 menu_coming_start[MENU_COUNT] = {};
    Uint32 menu_coming_until[MENU_COUNT] = {};
    int credit_hover = -1;
    int hint_hover = -1;
    int overlay_hover = -1;
    OverlayButton overlay_buttons[2] = {};
    int overlay_button_count = 0;
    int update_link_hover = -1;
    SDL_Rect update_link_hit = {0, 0, 0, 0};
    SDL_Rect hint_rects[MAX_HINTS] = {};
    HintAction hint_actions[MAX_HINTS] = {};
    int hint_count = 0;
};

// Данные кадра для отрисовки: ссылки на изменяемое состояние не хранятся,
// main передаёт их по значению на один вызов.
struct ScreenFrame {
    const Game* game = nullptr;
    const UpdateChecker* updates = nullptr;
    const std::string* app_version = nullptr;
    int score = 0;
    Uint32 igt_ms = 0;
    int best_score = 0;
    Uint32 now = 0;
    bool paused = false;
    bool game_over = false;
    bool won = false;
};

struct MenuLayout {
    SDL_Rect buttons[MENU_COUNT];
    int title_cy;
};

// ширина всех кнопок меню — по самому длинному пункту, чтобы текст
// никогда не вылезал за кнопку и пункты выглядели одинаково
int menu_button_width(const Ui& ui, int win_w);
MenuLayout menu_layout(const Ui& ui, int win_w, int win_h);

int hint_at(const ScreenState& st, const SDL_Point* pt, Screen screen);
int overlay_at(const ScreenState& st, const SDL_Point* pt, Screen screen, bool paused,
               bool game_over, bool won);
void credit_link_rects(const Ui& ui, int win_w, int win_h, SDL_Rect rects[CREDIT_LINK_COUNT]);

void render_screen(Ui& ui, ScreenState& st, const ScreenFrame& f);

}  // namespace ks
