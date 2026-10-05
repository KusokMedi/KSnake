#pragma once

#include <cstdint>
#include <deque>
#include <random>
#include <utility>

namespace ks {

struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
};

// Итог одного шага: main по нему пишет лог и решает, показывать ли Game Over.
enum class StepKind { Moved, AteFood, Won, WallCollision, SelfCollision };

struct StepOutcome {
    StepKind kind = StepKind::Moved;
    Point head{0, 0};
    int score = 0;
    uint32_t move_interval = 0;
};

// Игровая логика без SDL: поле, змейка, еда, очки и очередь поворотов.
class Game {
public:
    explicit Game(uint32_t seed);

    void reset();
    void queue_turn(int ndx, int ndy);
    StepOutcome step();

    const std::deque<Point>& snake() const { return snake_; }
    const Point& food() const { return food_; }
    int dir_x() const { return dx_; }
    int dir_y() const { return dy_; }
    int score() const { return score_; }
    uint32_t move_interval() const { return move_interval_; }

private:
    void place_food();

    std::deque<Point> snake_;
    std::deque<std::pair<int, int>> pending_turns_;
    std::mt19937 rng_;
    Point food_{0, 0};
    int dx_ = 1;
    int dy_ = 0;
    int score_ = 0;
    uint32_t move_interval_ = 150;
};

}  // namespace ks
