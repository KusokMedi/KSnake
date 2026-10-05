#include "screens.h"

#include <algorithm>

#include "util.h"

namespace ks {
namespace {

const uint8_t kBright[3] = {255, 255, 255};
const uint8_t kBody[3] = {225, 225, 225};
const uint8_t kDim[3] = {155, 155, 155};
const uint8_t kGood[3] = {46, 204, 113};
const uint8_t kBad[3] = {231, 76, 60};

void render_hints(Ui& ui, ScreenState& st, TTF_Font* font, int center_x, int center_y,
                  const HintPart* parts, int count, uint8_t alpha = 255) {
    st.hint_count = 0;
    int sep_w = 0, sep_h = 0;
    text_size(font, HINT_SEPARATOR, sep_w, sep_h);
    int total_w = 0;
    for (int i = 0; i < count; ++i) {
        int w = 0, h = 0;
        text_size(font, parts[i].text, w, h);
        total_w += w + (i + 1 < count ? sep_w : 0);
    }
    int x = center_x - total_w / 2;
    for (int i = 0; i < count && st.hint_count < MAX_HINTS; ++i) {
        int w = 0, h = 0;
        text_size(font, parts[i].text, w, h);
        bool hover = (st.hint_hover == st.hint_count);
        uint8_t r = hover ? 255 : 180;
        uint8_t g = hover ? 255 : 180;
        uint8_t b = hover ? 255 : 180;
        ui.render_text(font, parts[i].text, x + w / 2, center_y, r, g, b, alpha);
        if (hover) {
            ui.fill_rect(x, center_y + h / 2 + ui.px(3), w, ui.px(2), mix_bg(r, alpha),
                         mix_bg(g, alpha), mix_bg(b, alpha));
        }
        st.hint_rects[st.hint_count] = {x, center_y - h / 2, w, h};
        st.hint_actions[st.hint_count] = parts[i].action;
        ++st.hint_count;
        x += w;
        if (i + 1 < count) {
            ui.render_text(font, HINT_SEPARATOR, x + sep_w / 2, center_y, 120, 120, 120, alpha);
            x += sep_w;
        }
    }
}

void overlay_buttons_layout(Ui& ui, ScreenState& st, int panel_cx, int top, int count, int btn_h) {
    int bh = btn_h;
    int gap_x = ui.px(18);
    int bw = 0;
    for (int i = 0; i < count; ++i) {
        int tw = 0, th = 0;
        text_size(ui.button(), st.overlay_buttons[i].label, tw, th);
        bw = std::max(bw, tw + ui.px(52));
    }
    bw = std::max(1, std::min(bw, (ui.lay_w() - ui.px(40) - (count - 1) * gap_x) / count));
    int total_w = count * bw + (count - 1) * gap_x;
    int x = panel_cx - total_w / 2;
    for (int i = 0; i < count; ++i) {
        st.overlay_buttons[i].rect = {x, top, bw, bh};
        x += bw + gap_x;
    }
    st.overlay_button_count = count;
}

void render_overlay_buttons(Ui& ui, const ScreenState& st) {
    for (int i = 0; i < st.overlay_button_count; ++i) {
        const OverlayButton& b = st.overlay_buttons[i];
        bool hover = (i == st.overlay_hover);
        const SDL_Rect& r = b.rect;
        // та же палитра, что у Play/Exit в главном меню
        if (b.danger) {
            ui.fill_rect(r.x, r.y, r.w, r.h, hover ? 130 : 46, hover ? 40 : 22,
                         hover ? 40 : 22);
            if (hover) ui.stroke_rect(r.x, r.y, r.w, r.h, 255, 120, 120);
            ui.render_text(ui.button(), b.label, r.x + r.w / 2, r.y + r.h / 2,
                           hover ? 255 : 231, hover ? 120 : 76, hover ? 120 : 60);
        } else {
            ui.fill_rect(r.x, r.y, r.w, r.h, hover ? 56 : 20, hover ? 110 : 40,
                         hover ? 62 : 24);
            if (hover) ui.stroke_rect(r.x, r.y, r.w, r.h, 150, 255, 175);
            ui.render_text(ui.button(), b.label, r.x + r.w / 2, r.y + r.h / 2,
                           hover ? 150 : 46, hover ? 255 : 204, hover ? 175 : 113);
        }
    }
}

void draw_field(Ui& ui, const Game& game) {
    const int cell = ui.cell();
    const int offset_x = ui.offset_x();
    const int offset_y = ui.offset_y();

    // Draw border around playing field
    ui.stroke_rect(offset_x - ui.px(1), offset_y - ui.px(1), WIDTH * cell + 2 * ui.px(1),
                   HEIGHT * cell + 2 * ui.px(1), 70, 70, 70);

    for (int gy = 0; gy < HEIGHT; ++gy) {
        for (int gx = 0; gx < WIDTH; ++gx) {
            ui.draw_cell(gx, gy, (gx + gy) % 2 == 0 ? 0 : 24, (gx + gy) % 2 == 0 ? 0 : 24,
                         (gx + gy) % 2 == 0 ? 0 : 24);
        }
    }

    ui.draw_cell(game.food().x, game.food().y, 231, 76, 60);
    const std::deque<Point>& snake = game.snake();
    for (size_t i = 0; i < snake.size(); ++i) {
        if (i == 0) {
            // Head: brighter green
            ui.draw_cell(snake[i].x, snake[i].y, 46, 204, 113);
            // Draw eyes on head
            int eye_sz = std::max(2, cell / 7);
            int hx = offset_x + snake[i].x * cell;
            int hy = offset_y + snake[i].y * cell;
            if (game.dir_x() == 1) {
                ui.fill_rect(hx + cell - eye_sz * 2, hy + eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                ui.fill_rect(hx + cell - eye_sz * 2, hy + cell - eye_sz * 3, eye_sz, eye_sz, 20,
                             20, 20);
            } else if (game.dir_x() == -1) {
                ui.fill_rect(hx + eye_sz, hy + eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                ui.fill_rect(hx + eye_sz, hy + cell - eye_sz * 3, eye_sz, eye_sz, 20, 20, 20);
            } else if (game.dir_y() == 1) {
                ui.fill_rect(hx + eye_sz * 2, hy + cell - eye_sz * 2, eye_sz, eye_sz, 20, 20, 20);
                ui.fill_rect(hx + cell - eye_sz * 3, hy + cell - eye_sz * 2, eye_sz, eye_sz, 20,
                             20, 20);
            } else if (game.dir_y() == -1) {
                ui.fill_rect(hx + eye_sz * 2, hy + eye_sz, eye_sz, eye_sz, 20, 20, 20);
                ui.fill_rect(hx + cell - eye_sz * 3, hy + eye_sz, eye_sz, eye_sz, 20, 20, 20);
            }
        } else {
            // Body: slightly darker green
            ui.draw_cell(snake[i].x, snake[i].y, 39, 174, 96);
        }
    }
}

void draw_paused(Ui& ui, ScreenState& st) {
    const int cx = ui.lay_w() / 2;
    const int cy = ui.lay_h() / 2;
    ui.draw_dim(170);
    int pad = ui.px(20);

    st.overlay_buttons[0] = {"Continue", HintAction::Continue, {}, false};
    st.overlay_buttons[1] = {"Menu", HintAction::Menu, {}, true};
    int label_w = 0, label_h = 0;
    text_size(ui.title(), "Paused", label_w, label_h);

    int btn_h = ui.px(46);
    int gap = ui.px(20);
    int pw = std::max(1, std::min(ui.px(420), ui.lay_w() - ui.px(16)));
    int ph = std::max(1, std::min(2 * pad + label_h + gap + btn_h, ui.lay_h() - ui.px(16)));
    ui.fill_rect(cx - pw / 2, cy - ph / 2, pw, ph, 26, 26, 26);
    ui.stroke_rect(cx - pw / 2, cy - ph / 2, pw, ph, 70, 70, 70);

    int panel_top = cy - ph / 2;
    ui.render_text(ui.title(), "Paused", cx, panel_top + pad + label_h / 2, 255, 255, 255);
    overlay_buttons_layout(ui, st, cx, panel_top + pad + label_h + gap, 2, btn_h);
    render_overlay_buttons(ui, st);
}

void draw_game_over(Ui& ui, ScreenState& st, const ScreenFrame& f) {
    const int cx = ui.lay_w() / 2;
    const int cy = ui.lay_h() / 2;
    ui.draw_dim(160);

    int pad = ui.px(22);
    st.overlay_buttons[0] = {"Restart", HintAction::Restart, {}, false};
    st.overlay_buttons[1] = {"Menu", HintAction::Menu, {}, true};

    const char* title_text = f.won ? "You Won!" : "Game Over!";
    int tw = 0, th = 0, sh = 0, dd = 0;
    text_size(ui.gameover(), title_text, tw, th);
    text_size(ui.small(), "New Score: 0", dd, sh);

    int btn_h = ui.px(46);
    int gap = ui.px(20);
    int stats_h = 3 * (sh + ui.px(10));
    int content_h = th + gap + stats_h + gap + btn_h;
    int pw = std::max(1, std::min(ui.px(460), ui.lay_w() - ui.px(16)));
    int ph = std::max(1, std::min(2 * pad + content_h, ui.lay_h() - ui.px(16)));
    ui.fill_rect(cx - pw / 2, cy - ph / 2, pw, ph, 26, 26, 26);
    ui.stroke_rect(cx - pw / 2, cy - ph / 2, pw, ph, 70, 70, 70);

    int panel_top = cy - ph / 2;
    int title_cy = panel_top + pad + th / 2;
    int group_top = title_cy + th / 2 + gap;
    int line_step = sh + ui.px(10);

    int score_cy = group_top + sh / 2;
    int time_cy = score_cy + line_step;
    int best_cy = time_cy + line_step;

    overlay_buttons_layout(ui, st, cx, group_top + stats_h + gap, 2, btn_h);

    if (f.won) {
        ui.render_text(ui.gameover(), title_text, cx, title_cy, 46, 204, 113);
    } else {
        ui.render_text(ui.gameover(), title_text, cx, title_cy, 231, 76, 60);
    }
    ui.render_text(ui.small(), ("New Score: " + std::to_string(f.score)).c_str(), cx, score_cy,
                   255, 255, 255);
    int tm = static_cast<int>(f.igt_ms / 1000);
    ui.render_text(ui.small(),
                   ("Time: " + std::to_string(tm / 60) + ":" + (tm % 60 < 10 ? "0" : "") +
                    std::to_string(tm % 60))
                       .c_str(),
                   cx, time_cy, 255, 255, 255);
    ui.render_text(ui.small(), ("Best: " + std::to_string(f.best_score)).c_str(), cx, best_cy,
                   255, 255, 255);
    render_overlay_buttons(ui, st);
}

void draw_exit_prompt(Ui& ui, ScreenState& st) {
    ui.draw_dim(150);

    const int cx = ui.lay_w() / 2;
    const int cy = ui.lay_h() / 2;
    int ew = std::max(1, std::min(ui.px(480), ui.lay_w() - ui.px(16)));
    int eh = std::max(1, std::min(ui.px(160), ui.lay_h() - ui.px(16)));
    ui.fill_rect(cx - ew / 2, cy - eh / 2, ew, eh, 26, 26, 26);
    ui.stroke_rect(cx - ew / 2, cy - eh / 2, ew, eh, 70, 70, 70);
    int w1 = 0, w2 = 0, h1 = 0, h2 = 0;
    text_size(ui.button(), "Do you want to ", w1, h1);
    text_size(ui.button(), "exit?", w2, h2);
    int start = cx - (w1 + w2) / 2;
    int line_cy = cy - ui.px(30);
    ui.render_text(ui.button(), "Do you want to ", start + w1 / 2, line_cy, 255, 255, 255);
    ui.render_text(ui.button(), "exit?", start + w1 + w2 / 2, line_cy, 231, 76, 60);

    static const HintPart exit_hints[] = {
        {"Enter to exit", HintAction::Confirm},
        {"Esc to cancel", HintAction::Cancel},
    };
    render_hints(ui, st, ui.small(), cx, cy + ui.px(38), exit_hints, 2);
}

void draw_credits(Ui& ui, ScreenState& st) {
    ui.draw_dim(150);

    const int cx = ui.lay_w() / 2;
    const int cy = ui.lay_h() / 2;
    int cw = std::max(1, std::min(ui.px(520), ui.lay_w() - ui.px(16)));
    int ch = std::max(1, std::min(ui.px(280), ui.lay_h() - ui.px(16)));
    ui.fill_rect(cx - cw / 2, cy - ch / 2, cw, ch, 26, 26, 26);
    ui.stroke_rect(cx - cw / 2, cy - ch / 2, cw, ch, 70, 70, 70);

    ui.render_text(ui.title(), "Credits", cx, cy - ui.px(95), 255, 255, 255);
    SDL_Rect link_rects[CREDIT_LINK_COUNT];
    credit_link_rects(ui, ui.lay_w(), ui.lay_h(), link_rects);
    for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
        int wl = 0, hl = 0, wu = 0, hu = 0;
        text_size(ui.small(), CREDIT_LINKS[i].label, wl, hl);
        text_size(ui.small(), CREDIT_LINKS[i].url, wu, hu);
        int x0 = link_rects[i].x - wl;
        int ly = link_rects[i].y + link_rects[i].h / 2;
        int yu = link_rects[i].y;
        bool hover = (i == st.credit_hover);
        ui.render_text(ui.small(), CREDIT_LINKS[i].label, x0 + wl / 2, ly, 140, 140, 140);
        if (hover) {
            ui.render_text(ui.small(), CREDIT_LINKS[i].url, link_rects[i].x + wu / 2, ly, 232,
                           232, 232);
            ui.fill_rect(link_rects[i].x, yu + hu + ui.px(2), wu, ui.px(2), 232, 232, 232);
        } else {
            ui.render_text(ui.small(), CREDIT_LINKS[i].url, link_rects[i].x + wu / 2, ly, 180,
                           180, 180);
        }
    }
    ui.render_text(ui.small(), "Powered by SDL2 & SDL2_ttf (C++)", cx, cy + ui.px(75), 150, 150,
                   150);
    static const HintPart credits_hints[] = {
        {"Any key to return", HintAction::Menu},
    };
    render_hints(ui, st, ui.small(), cx, cy + ui.px(105), credits_hints, 1);
}

void draw_updates(Ui& ui, ScreenState& st, const UpdateChecker& updates,
                  const std::string& app_version) {
    ui.draw_dim(150);

    const int cx = ui.lay_w() / 2;
    const int cy = ui.lay_h() / 2;
    const int win_w = ui.lay_w();
    const int win_h = ui.lay_h();

    static const HintPart back_hints[] = {
        {"Any key to go back", HintAction::Menu},
    };

    int local[3] = {0, 0, 0};
    parse_version(app_version, local);
    std::string local_str = format_version(local);

    std::string status_text;
    std::string version_line;
    uint8_t status_r = kBright[0], status_g = kBright[1], status_b = kBright[2];
    std::string note;
    bool have_link = false;

    st.update_link_hit = {0, 0, 0, 0};
    switch (updates.state()) {
        case UpdateState::Checking: {
            int dots = static_cast<int>((SDL_GetTicks() / 400) % 4);
            status_text = "Checking for updates" + std::string(dots, '.');
            version_line = "Your version is " + local_str;
            break;
        }
        case UpdateState::UpToDate:
            status_text = "You are up to date";
            status_r = kGood[0];
            status_g = kGood[1];
            status_b = kGood[2];
            version_line = "Your version is " + local_str;
            break;
        case UpdateState::Ahead:
            // локальная сборка новее последнего релиза — обновляться не нужно
            status_text = "Unstable build";
            status_r = 255;
            status_g = 200;
            status_b = 80;
            version_line = "Your version is " + local_str;
            if (!updates.version().empty()) {
                version_line += ", but latest is " + updates.version();
            }
            break;
        case UpdateState::Available:
        case UpdateState::Unknown:
            status_text = "You need to update";
            status_r = kGood[0];
            status_g = kGood[1];
            status_b = kGood[2];
            version_line = "Your version is " + local_str;
            if (!updates.version().empty()) {
                version_line += ", but latest is " + updates.version();
            }
            have_link = !updates.url().empty();
            break;
        default:
            status_text = "Update check failed";
            status_r = kBad[0];
            status_g = kBad[1];
            status_b = kBad[2];
            note = updates.note().empty()
                       ? "Check your internet connection and try again"
                       : updates.note();
            break;
    }

    // ---- геометрия: панель по контенту, равные интервалы ----
    const int pad = ui.px(28);
    const int btn_h = ui.px(46);
    const int btn_pad = ui.px(28);
    int title_gap = ui.px(14);
    int block_gap = ui.px(22);
    int hint_gap = ui.px(24);

    int title_w = 0, title_h = 0;
    text_size(ui.gameover(), "Updates", title_w, title_h);
    int status_w = 0, status_h = 0;
    if (!status_text.empty()) text_size(ui.button(), status_text.c_str(), status_w, status_h);

    int ver_w = 0, ver_h = 0;
    if (!version_line.empty()) text_size(ui.small(), version_line.c_str(), ver_w, ver_h);
    int note_w = 0, note_h = 0;
    if (!note.empty()) text_size(ui.small(), note.c_str(), note_w, note_h);

    int btn_w = 0, link_w = 0, link_h = 0;
    if (have_link) {
        text_size(ui.button(), "Open release page", link_w, link_h);
        btn_w = link_w + 2 * btn_pad;
    }

    int hint_w = 0, hint_h = 0;
    text_size(ui.small(), back_hints[0].text, hint_w, hint_h);

    auto total_height = [&]() {
        int h_total = title_h;
        if (!status_text.empty()) h_total += title_gap + status_h;
        if (!version_line.empty()) h_total += block_gap + ver_h;
        if (!note.empty()) h_total += block_gap + note_h;
        if (have_link) h_total += block_gap + btn_h;
        return h_total + hint_gap + hint_h;
    };

    int content_h = total_height();

    // в низком окне равномерно поджимаем интервалы, чтобы панель не обрезалась
    int avail_h = win_h - ui.px(16) - 2 * pad;
    if (content_h > avail_h) {
        double k = static_cast<double>(avail_h) / static_cast<double>(content_h);
        title_gap = std::max(ui.px(5), static_cast<int>(title_gap * k));
        block_gap = std::max(ui.px(8), static_cast<int>(block_gap * k));
        hint_gap = std::max(ui.px(10), static_cast<int>(hint_gap * k));
        content_h = total_height();
    }

    // ширина панели фиксированная: текст не должен «прыгать» при смене состояния
    int pw = std::max(1, std::min(ui.px(520), win_w - ui.px(16)));
    int text_limit = pw - 2 * pad - ui.px(12);
    status_text = ui.ellipsize(ui.button(), status_text, text_limit);
    version_line = ui.ellipsize(ui.small(), version_line, text_limit);
    note = ui.ellipsize(ui.small(), note, text_limit);

    int ph = std::max(1, std::min(content_h + 2 * pad, win_h - ui.px(16)));
    ui.fill_rect(cx - pw / 2, cy - ph / 2, pw, ph, 26, 26, 26);
    ui.stroke_rect(cx - pw / 2, cy - ph / 2, pw, ph, 70, 70, 70);

    int panel_top = cy - ph / 2;
    int y = panel_top + pad;

    ui.render_text(ui.gameover(), "Updates", cx, y + title_h / 2, kBright[0], kBright[1],
                   kBright[2]);
    y += title_h;

    if (!status_text.empty()) {
        y += title_gap;
        ui.render_text(ui.button(), status_text.c_str(), cx, y + status_h / 2, status_r,
                       status_g, status_b);
        y += status_h;
    }

    if (!version_line.empty()) {
        y += block_gap;
        ui.render_text(ui.small(), version_line.c_str(), cx, y + ver_h / 2, kDim[0], kDim[1],
                       kDim[2]);
        y += ver_h;
    }

    if (!note.empty()) {
        y += block_gap;
        ui.render_text(ui.small(), note.c_str(), cx, y + note_h / 2, kBody[0], kBody[1],
                       kBody[2]);
        y += note_h;
    }

    if (have_link) {
        y += block_gap;
        int bx = cx - btn_w / 2;
        bool hover = st.update_link_hover == 0;
        ui.fill_rect(bx, y, btn_w, btn_h, hover ? 56 : 20, hover ? 110 : 40, hover ? 62 : 24);
        if (hover) ui.stroke_rect(bx, y, btn_w, btn_h, 150, 255, 175);
        ui.render_text(ui.button(), "Open release page", cx, y + btn_h / 2, hover ? 150 : 46,
                       hover ? 255 : 204, hover ? 175 : 113);
        st.update_link_hit = {bx, y, btn_w, btn_h};
        y += btn_h;
    }

    y += hint_gap;
    render_hints(ui, st, ui.small(), cx, y + hint_h / 2, back_hints, 1);
}

void draw_menu(Ui& ui, ScreenState& st, const ScreenFrame& f) {
    const int win_w = ui.lay_w();
    const int win_h = ui.lay_h();
    MenuLayout ml = menu_layout(ui, win_w, win_h);
    int btn_h = ml.buttons[0].h;
    int title_cy = ml.title_cy;
    float glow = f.updates ? f.updates->glow() : 0.0f;

    ui.render_text(ui.title(), "KSnake", win_w / 2, title_cy, 255, 255, 255);

    auto draw_button = [&](const char* label, int center_y, int index) {
        int bw = menu_button_width(ui, win_w);
        int bh = btn_h;
        bool active = (index == st.menu_index);
        bool is_coming = st.menu_coming[index] && f.now < st.menu_coming_until[index];

        float progress = 0.0f;
        if (is_coming) {
            progress =
                static_cast<float>(f.now - st.menu_coming_start[index]) / 1500.0f;
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;
        }

        int x0 = win_w / 2 - bw / 2;
        int y0 = center_y - bh / 2;

        if (index == MENU_PLAY) {
            uint8_t bg_r = active ? 56 : 20;
            uint8_t bg_g = active ? 110 : 40;
            uint8_t bg_b = active ? 62 : 24;
            uint8_t tx_r = active ? 150 : 46;
            uint8_t tx_g = active ? 255 : 204;
            uint8_t tx_b = active ? 175 : 113;
            ui.fill_rect(x0, y0, bw, bh, bg_r, bg_g, bg_b);
            if (active) ui.stroke_rect(x0, y0, bw, bh, tx_r, tx_g, tx_b);
            ui.render_text(ui.button(), label, win_w / 2, center_y, tx_r, tx_g, tx_b);
        } else if (index == MENU_EXIT) {
            uint8_t bg_r = active ? 130 : 46;
            uint8_t bg_g = active ? 40 : 22;
            uint8_t bg_b = active ? 40 : 22;
            uint8_t tx_r = active ? 255 : 231;
            uint8_t tx_g = active ? 120 : 76;
            uint8_t tx_b = active ? 120 : 60;
            ui.fill_rect(x0, y0, bw, bh, bg_r, bg_g, bg_b);
            if (active) ui.stroke_rect(x0, y0, bw, bh, tx_r, tx_g, tx_b);
            ui.render_text(ui.button(), label, win_w / 2, center_y, tx_r, tx_g, tx_b);
        } else {
            // обычные пункты: подсветка при наведении, как у Play и Exit,
            // а у Check for updates сверху подмешивается жёлтый, если есть обновление
            float g = (index == MENU_UPDATES) ? glow : 0.0f;
            float h = active ? 1.0f : 0.0f;
            auto mixh = [&](uint8_t rest_c, uint8_t hover_c) {
                return static_cast<uint8_t>(rest_c + (hover_c - rest_c) * h);
            };
            auto mixc = [&](uint8_t from_c, uint8_t to_c) {
                return static_cast<uint8_t>(from_c + (to_c - from_c) * g);
            };
            ui.fill_rect(x0, y0, bw, bh, mixc(mixh(30, 54), 92), mixc(mixh(30, 56), 80),
                         mixc(mixh(34, 60), 10));
            if (active) {
                ui.stroke_rect(x0, y0, bw, bh, mixc(mixh(190, 205), 255),
                               mixc(mixh(190, 205), 228), mixc(mixh(190, 210), 96));
            }
            if (is_coming) {
                ui.render_text(ui.button(), "Coming soon..", win_w / 2, center_y, 200, 205, 200,
                               static_cast<uint8_t>(255.0f * (1.0f - progress)));
                ui.render_text(ui.button(), label, win_w / 2, center_y, active ? 190 : 130,
                               active ? 190 : 130, active ? 190 : 130,
                               static_cast<uint8_t>(255.0f * progress));
            } else {
                ui.render_text(ui.button(), label, win_w / 2, center_y, mixc(mixh(130, 205), 255),
                               mixc(mixh(130, 205), 216), mixc(mixh(130, 210), 64));
            }
        }
    };

    for (int i = 0; i < MENU_COUNT; ++i) {
        int btn_cy = ml.buttons[i].y + ml.buttons[i].h / 2;
        draw_button(MENU_ITEMS[i].label, btn_cy, i);
        if (st.menu_coming[i] && f.now >= st.menu_coming_until[i]) {
            st.menu_coming[i] = false;
        }
    }
}

}  // namespace

const MenuItem MENU_ITEMS[MENU_COUNT] = {
    {"Play", false},
    {"Skins", true},
    {"Settings", true},
    {"Credits", false},
    {"Check for updates", false},
    {"Exit", false},
};

const CreditLink CREDIT_LINKS[CREDIT_LINK_COUNT] = {
    {"My github: ", "https://github.com/kusokmedi", -40},
    {"Source code: ", "https://github.com/KusokMedi/KSnake", -12},
    {"Telegram: ", "https://t.me/kusokmedi52", 16},
    {"About me: ", "https://kusokmedi.lat", 44},
};

int menu_button_width(const Ui& ui, int win_w) {
    int pad_x = ui.px(28);
    int bw = ui.px(230);
    for (int i = 0; i < MENU_COUNT; ++i) {
        int tw = 0, th = 0;
        text_size(ui.button(), MENU_ITEMS[i].label, tw, th);
        bw = std::max(bw, tw + 2 * pad_x);
    }
    return std::max(1, std::min(bw, win_w - ui.px(24)));
}

MenuLayout menu_layout(const Ui& ui, int win_w, int win_h) {
    int margin = ui.px(8);
    int btn_h = ui.px(48);
    int gap = ui.px(20);
    int head_step = ui.px(80);  // title centre -> first button centre
    int header = ui.px(40);     // top -> title centre
    int avail = std::max(win_h - 2 * margin, 1);
    int total = header + head_step + (btn_h + gap) * (MENU_COUNT - 1) + btn_h / 2;
    if (total > avail) {
        float s = static_cast<float>(avail) / static_cast<float>(total);
        header = std::max(1, static_cast<int>(header * s));
        head_step = std::max(1, static_cast<int>(head_step * s));
        btn_h = std::max(1, static_cast<int>(btn_h * s));
        gap = std::max(1, static_cast<int>(gap * s));
        total = header + head_step + (btn_h + gap) * (MENU_COUNT - 1) + btn_h / 2;
    }
    int top = std::max((win_h - total) / 2, margin);
    MenuLayout ml;
    ml.title_cy = top + header;
    int bw = menu_button_width(ui, win_w);
    int cy = ml.title_cy + head_step;
    for (int i = 0; i < MENU_COUNT; ++i) {
        ml.buttons[i] = {win_w / 2 - bw / 2, cy - btn_h / 2, bw, btn_h};
        cy += btn_h + gap;
    }
    return ml;
}

int hint_at(const ScreenState& st, const SDL_Point* pt, Screen screen) {
    if (screen != st.screen) return -1;
    for (int i = 0; i < st.hint_count; ++i) {
        if (SDL_PointInRect(pt, &st.hint_rects[i])) return i;
    }
    return -1;
}

int overlay_at(const ScreenState& st, const SDL_Point* pt, Screen screen, bool paused,
               bool game_over, bool won) {
    if (screen != Screen::Game) return -1;
    if (!(paused || game_over || won)) return -1;
    for (int i = 0; i < st.overlay_button_count; ++i) {
        if (SDL_PointInRect(pt, &st.overlay_buttons[i].rect)) return i;
    }
    return -1;
}

void credit_link_rects(const Ui& ui, int win_w, int win_h, SDL_Rect rects[CREDIT_LINK_COUNT]) {
    int cx = win_w / 2;
    int cy = win_h / 2;
    for (int i = 0; i < CREDIT_LINK_COUNT; ++i) {
        int wl = 0, hl = 0, wu = 0, hu = 0;
        text_size(ui.small(), CREDIT_LINKS[i].label, wl, hl);
        text_size(ui.small(), CREDIT_LINKS[i].url, wu, hu);
        int x0 = cx - (wl + wu) / 2;
        int y = cy + ui.px(CREDIT_LINKS[i].dy) - hu / 2;
        rects[i] = {x0 + wl, y, wu, hu};
    }
}

void render_screen(Ui& ui, ScreenState& st, const ScreenFrame& f) {
    if (st.screen != Screen::Game && st.screen != Screen::ExitPrompt) {
        ui.draw_checker(ui.lay_w(), ui.lay_h());
    }

    switch (st.screen) {
        case Screen::Game:
            draw_field(ui, *f.game);
            if (f.paused) {
                draw_paused(ui, st);
            } else if (f.game_over || f.won) {
                draw_game_over(ui, st, f);
            }
            break;
        case Screen::ExitPrompt:
            draw_exit_prompt(ui, st);
            break;
        case Screen::Credits:
            draw_credits(ui, st);
            break;
        case Screen::Update:
            draw_updates(ui, st, *f.updates, *f.app_version);
            break;
        case Screen::Menu:
            draw_menu(ui, st, f);
            break;
    }
}

}  // namespace ks