#include "ui.h"

#include <algorithm>
#include <cmath>

namespace ks {

void text_size(TTF_Font* font, const char* text, int& w, int& h) {
    w = 0;
    h = 0;
    if (font) TTF_SizeUTF8(font, text, &w, &h);
}

uint8_t mix_bg(uint8_t c, uint8_t a) {
    return static_cast<uint8_t>((c * a + 26 * (255 - a)) / 255);
}

Ui::Ui(SDL_Renderer* renderer, uint32_t seed) : renderer_(renderer), rng_(seed) {}

void Ui::set_logger(LogFn log) {
    log_ = std::move(log);
}

Ui::~Ui() {
    destroy();
}

TTF_Font* Ui::load_font(int size) {
    for (const char* path : FONT_CANDIDATES) {
        TTF_Font* f = TTF_OpenFont(path, size);
        if (f) {
            if (log_) log_("FONT path=" + std::string(path) + " size=" + std::to_string(size));
            return f;
        }
    }
    return nullptr;
}

TTF_Font* Ui::font_at(int base) {
    int size = std::max(8, static_cast<int>(std::lround(base * scale_)));
    auto it = font_cache_.find(size);
    if (it != font_cache_.end()) return it->second;
    TTF_Font* f = load_font(size);
    font_cache_[size] = f;
    return f;
}

void Ui::refresh_fonts() {
    font_title_ = font_at(64);
    font_button_ = font_at(26);
    font_small_ = font_at(16);
    font_gameover_ = font_at(48);
}

bool Ui::load_fonts() {
    refresh_fonts();
    return font_title_ && font_button_ && font_small_ && font_gameover_;
}

void Ui::destroy() {
    drop_text_cache();
    for (auto& kv : font_cache_) {
        if (kv.second) TTF_CloseFont(kv.second);
    }
    font_cache_.clear();
    font_title_ = nullptr;
    font_button_ = nullptr;
    font_small_ = nullptr;
    font_gameover_ = nullptr;
}

int Ui::px(int base) const {
    int v = static_cast<int>(std::lround(base * scale_));
    if (v == 0) v = base < 0 ? -1 : 1;
    return v;
}

void Ui::drop_text_cache() {
    for (auto& kv : text_cache_) {
        if (kv.second.tex) SDL_DestroyTexture(kv.second.tex);
    }
    text_cache_.clear();
}

void Ui::compute_layout(int win_w, int win_h) {
    if (win_w < 1) win_w = 1;
    if (win_h < 1) win_h = 1;
    int pad = 16;
    cell_ = std::min((win_w - 2 * pad) / WIDTH, (win_h - 2 * pad) / HEIGHT);
    if (cell_ < 1) cell_ = 1;
    offset_x_ = (win_w - cell_ * WIDTH) / 2;
    offset_y_ = (win_h - cell_ * HEIGHT) / 2;
    if (offset_x_ < pad) offset_x_ = pad;
    if (offset_y_ < pad) offset_y_ = pad;
}

void Ui::apply_layout(int win_w, int win_h) {
    lay_w_ = win_w < 1 ? 1 : win_w;
    lay_h_ = win_h < 1 ? 1 : win_h;
    float s = std::min(static_cast<float>(lay_w_) / static_cast<float>(WIN_W),
                       static_cast<float>(lay_h_) / static_cast<float>(WIN_H));
    s = std::floor(s * 20.0f) / 20.0f;  // шаг 5%: реже пересоздаём шрифты
    float next = std::max(0.6f, std::min(2.0f, s));
    if (std::fabs(next - scale_) > 0.001f) {
        scale_ = next;
        refresh_fonts();
        drop_text_cache();
    }
    compute_layout(lay_w_, lay_h_);
}

void Ui::shuffle_checker() {
    if (CHECKER_SQ < 1) {
        checker_dx_ = checker_dy_ = 0;
        return;
    }
    std::uniform_int_distribution<int> dist(0, CHECKER_SQ - 1);
    checker_dx_ = dist(rng_);
    checker_dy_ = dist(rng_);
}

void Ui::fill_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    SDL_Rect rect{x, y, w, h};
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    SDL_RenderFillRect(renderer_, &rect);
}

void Ui::stroke_rect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
    SDL_SetRenderDrawColor(renderer_, r, g, b, 255);
    SDL_Rect rect{x, y, w, h};
    SDL_RenderDrawRect(renderer_, &rect);
}

void Ui::draw_dim(uint8_t alpha) {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, alpha);
    SDL_Rect all{0, 0, lay_w_, lay_h_};
    SDL_RenderFillRect(renderer_, &all);
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
}

void Ui::draw_checker(int w, int h, int sq) {
    if (sq < 1) sq = 1;
    // Клетка i занимает [dx + i*sq, ...), поэтому сетка может начинаться
    // со случайного сдвига и не привязываться к краю окна.
    int i0 = static_cast<int>(std::floor(-static_cast<double>(checker_dx_) / sq));
    int j0 = static_cast<int>(std::floor(-static_cast<double>(checker_dy_) / sq));
    for (int j = j0; j * sq + checker_dy_ < h; ++j) {
        int y = j * sq + checker_dy_;
        int fh = std::min(sq, h - y);
        if (fh < 1) break;
        for (int i = i0; i * sq + checker_dx_ < w; ++i) {
            int x = i * sq + checker_dx_;
            int fw = std::min(sq, w - x);
            if (fw < 1) break;
            uint8_t c = (((i + j) % 2 + 2) % 2 == 0) ? 8 : 16;
            fill_rect(x, y, fw, fh, c, c, c);
        }
    }
}

void Ui::draw_cell(int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    fill_rect(offset_x_ + x * cell_, offset_y_ + y * cell_, cell_, cell_, r, g, b);
}

std::string Ui::ellipsize(TTF_Font* font, const std::string& text, int max_w) const {
    if (max_w <= 0 || !font) return text;
    int tw = 0, th = 0;
    text_size(font, text.c_str(), tw, th);
    if (tw <= max_w) return text;
    const char* dots = "...";
    int dw = 0, dh = 0;
    text_size(font, dots, dw, dh);
    std::string out;
    size_t i = 0;
    while (i < text.size()) {
        size_t next = i + 1;
        unsigned char c = static_cast<unsigned char>(text[i]);
        int extra = 0;
        if (c >= 0xF0) extra = 3;
        else if (c >= 0xE0) extra = 2;
        else if (c >= 0xC0) extra = 1;
        if (i + extra < text.size()) next = i + extra + 1;
        out.append(text, i, next - i);
        int w2 = 0, h2 = 0;
        text_size(font, out.c_str(), w2, h2);
        if (w2 + dw > max_w) {
            out.resize(i);
            break;
        }
        i = next;
    }
    return out + dots;
}

void Ui::render_text(TTF_Font* font, const char* text, int center_x, int center_y,
                     uint8_t r, uint8_t g, uint8_t b, uint8_t alpha) {
    if (!font || !text) return;

    char key[48];
    snprintf(key, sizeof(key), "%p|%u,%u,%u|", static_cast<const void*>(font),
             static_cast<unsigned>(r), static_cast<unsigned>(g),
             static_cast<unsigned>(b));
    std::string cache_key = std::string(key) + text;

    auto it = text_cache_.find(cache_key);
    if (it == text_cache_.end()) {
        if (text_cache_.size() >= 64) drop_text_cache();
        SDL_Color color{r, g, b, 255};
        SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
        if (!surf) return;
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer_, surf);
        CachedText ct{tex, surf->w, surf->h};
        SDL_FreeSurface(surf);
        if (!tex) return;
        it = text_cache_.emplace(std::move(cache_key), ct).first;
    }

    SDL_SetTextureAlphaMod(it->second.tex, alpha);
    SDL_Rect dst{center_x - it->second.w / 2, center_y - it->second.h / 2,
                 it->second.w, it->second.h};
    SDL_RenderCopy(renderer_, it->second.tex, nullptr, &dst);
}

}  // namespace ks