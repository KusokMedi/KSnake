#pragma once

#include <SDL.h>
#include <SDL_ttf.h>

#include <cstdint>
#include <functional>
#include <map>
#include <random>
#include <string>

#include "config.h"

namespace ks {

void text_size(TTF_Font* font, const char* text, int& w, int& h);

// Цвет текста, смешанный с подложкой панели по её альфе.
uint8_t mix_bg(uint8_t c, uint8_t a);

// Шрифты, масштаб интерфейса и примитивы отрисовки. Окно = холст: всё рисуется
// 1:1 (текст не мылится), поэтому общий масштаб интерфейса применяется к
// размерам шрифтов и отступам, а не к холсту.
class Ui {
public:
    // Сообщения в лог отладки (--debug), который ведёт main.
    using LogFn = std::function<void(const std::string&)>;

    Ui(SDL_Renderer* renderer, uint32_t seed);
    ~Ui();

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    void set_logger(LogFn log);

    // false — ни один шрифт не открылся, игру запускать бессмысленно.
    bool load_fonts();
    // Освобождает шрифты и текстуры; вызывать до SDL_DestroyRenderer.
    void destroy();

    void apply_layout(int win_w, int win_h);
    void shuffle_checker();

    int px(int base) const;
    float scale() const { return scale_; }
    int lay_w() const { return lay_w_; }
    int lay_h() const { return lay_h_; }
    int cell() const { return cell_; }
    int offset_x() const { return offset_x_; }
    int offset_y() const { return offset_y_; }

    TTF_Font* title() const { return font_title_; }
    TTF_Font* button() const { return font_button_; }
    TTF_Font* small() const { return font_small_; }
    TTF_Font* gameover() const { return font_gameover_; }

    void render_text(TTF_Font* font, const char* text, int center_x, int center_y,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t alpha = 255);
    std::string ellipsize(TTF_Font* font, const std::string& text, int max_w) const;

    void fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
    void stroke_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
    void draw_dim(uint8_t alpha);
    void draw_checker(int w, int h, int sq = CHECKER_SQ);
    void draw_cell(int x, int y, uint8_t r, uint8_t g, uint8_t b);

private:
    struct CachedText {
        SDL_Texture* tex;
        int w;
        int h;
    };

    TTF_Font* load_font(int size);
    TTF_Font* font_at(int base);
    void refresh_fonts();
    void drop_text_cache();
    void compute_layout(int win_w, int win_h);

    SDL_Renderer* renderer_;
    LogFn log_;
    float scale_ = 1.0f;
    std::map<int, TTF_Font*> font_cache_;
    std::map<std::string, CachedText> text_cache_;
    TTF_Font* font_title_ = nullptr;
    TTF_Font* font_button_ = nullptr;
    TTF_Font* font_small_ = nullptr;
    TTF_Font* font_gameover_ = nullptr;
    int cell_ = CELL;
    int offset_x_ = 0;
    int offset_y_ = 0;
    int lay_w_ = WIN_W;
    int lay_h_ = WIN_H;
    int checker_dx_ = 0;
    int checker_dy_ = 0;
    std::mt19937 rng_;
};

}  // namespace ks
