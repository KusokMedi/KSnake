#include "game.h"

#include <algorithm>

#include "config.h"

namespace ks {
namespace {

std::deque<Point> initial_snake() {
    return {{WIDTH / 2, HEIGHT / 2},
            {WIDTH / 2 - 1, HEIGHT / 2},
            {WIDTH / 2 - 2, HEIGHT / 2}};
}

}  // namespace

Game::Game(uint32_t seed) : snake_(initial_snake()), rng_(seed) {
    place_food();
}

void Game::reset() {
    snake_ = initial_snake();
    dx_ = 1;
    dy_ = 0;
    pending_turns_.clear();
    score_ = 0;
    move_interval_ = 150;
    place_food();
}

void Game::queue_turn(int ndx, int ndy) {
    int ex = dx_, ey = dy_;
    if (!pending_turns_.empty()) {
        ex = pending_turns_.back().first;
        ey = pending_turns_.back().second;
    }
    if (ndx == ex && ndy == ey) return;
    if (ndx == -ex && ndy == -ey) return;
    // Ровно два поворота: столько змейка успевает съесть за один тик, третий
    // она всё равно не выполнит, но запомнит лишнее и «провалится» позже.
    if (pending_turns_.size() < 2) pending_turns_.push_back({ndx, ndy});
}

void Game::place_food() {
    if (static_cast<int>(snake_.size()) >= WIDTH * HEIGHT) return;
    std::uniform_int_distribution<int> dist_w(0, WIDTH - 1);
    std::uniform_int_distribution<int> dist_h(0, HEIGHT - 1);
    do {
        food_ = {dist_w(rng_), dist_h(rng_)};
    } while (std::find(snake_.begin(), snake_.end(), food_) != snake_.end());
}

StepOutcome Game::step() {
    if (!pending_turns_.empty()) {
        dx_ = pending_turns_.front().first;
        dy_ = pending_turns_.front().second;
        pending_turns_.pop_front();
    }
    Point head = snake_.front();
    head.x += dx_;
    head.y += dy_;

    StepOutcome out;
    out.head = head;
    out.score = score_;
    out.move_interval = move_interval_;

    // Check wall collision before pushing to avoid rendering out-of-bounds
    if (head.x < 0 || head.x >= WIDTH || head.y < 0 || head.y >= HEIGHT) {
        out.kind = StepKind::WallCollision;
        return out;
    }

    // Check self collision before pushing; the tail cell is free unless the
    // snake grows on this step
    auto body_end = (head == food_) ? snake_.end() : snake_.end() - 1;
    if (std::any_of(snake_.begin(), body_end, [&](const Point& p) { return p == head; })) {
        out.kind = StepKind::SelfCollision;
        return out;
    }

    snake_.push_front(head);
    if (head == food_) {
        ++score_;
        move_interval_ = static_cast<uint32_t>(std::max(60, 150 - score_ * 3));
        // Новое яблоко сразу после съеденного. Без этого food_ навсегда остаётся
        // на клетке, которую только что заняла голова: яблоко видно, но оно внутри
        // змейки и больше никогда не съедается. На заполненном поле place_food()
        // ничего не меняет, поэтому Won ниже всё ещё определяется верно.
        place_food();
        out.kind = static_cast<int>(snake_.size()) >= WIDTH * HEIGHT ? StepKind::Won
                                                                   : StepKind::AteFood;
    } else {
        snake_.pop_back();
    }
    return out;
}

}  // namespace ks
