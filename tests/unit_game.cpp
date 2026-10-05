// Юнит-тесты игровой логики KSnake (game.cpp не зависит от SDL).
// Сборка: ./tests/build_unit.sh  →  ./tests/build/unit_game
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <string>
#include <vector>

// Тесту нужен доступ к полям змейки; в рабочем коде они приватные.
#define private public
#include "../src/game.h"
#undef private

using namespace ks;

namespace {

int g_pass = 0;
int g_fail = 0;
std::vector<std::string> g_failed;

void check(bool ok, const std::string& what) {
    if (ok) {
        ++g_pass;
    } else {
        ++g_fail;
        g_failed.push_back(what);
        std::printf("FAIL: %s\n", what.c_str());
    }
}

#define CHECK(...) check(__VA_ARGS__)

bool g_empty(const Game& g) { return g.pending_turns_.empty(); }

void clear_turns(Game* g) {
    while (!g->pending_turns_.empty()) g->pending_turns_.pop_front();
}

// Ставит змейку в заданный путь от головы назад.
void set_body(Game* g, const std::vector<Point>& cells) {
    g->snake_.clear();
    for (const Point& p : cells) g->snake_.push_back(p);
}

// Клетка перед головой по текущему направлению.
// Змеевидный путь длиной n клеток по полю 20x15, начиная с (sx, sy) и двигаясь
// вправо. Возвращает клетки от головы к хвосту.
std::vector<Point> straight_path(int sx, int sy, int n, int dx, int dy) {
    std::vector<Point> cells;
    for (int i = 0; i < n; ++i) cells.push_back({sx + i * dx, sy + i * dy});
    return cells;
}

// Гамильтонов путь «змейка» по всему полю 20x15: соседние клетки всегда рядом.
const std::vector<Point>& hamilton_path() {
    static const std::vector<Point> p = [] {
        std::vector<Point> v;
        for (int y = 0; y < 15; ++y) {
            if (y % 2 == 0) {
                for (int x = 0; x < 20; ++x) v.push_back({x, y});
            } else {
                for (int x = 19; x >= 0; --x) v.push_back({x, y});
            }
        }
        return v;
    }();
    return p;
}

// Кладёт змейку вдоль гамильтонова пути: голова на path[len-1], хвост на
// path[0], еда на path[len]. Один шаг после этого тянет змейку на path[len].
void set_along_path(Game* g, size_t len, Point* food_out) {
    const std::vector<Point>& p = hamilton_path();
    std::vector<Point> cells;
    for (size_t i = len; i-- > 0;) cells.push_back(p[i]);
    set_body(g, cells);
    // змейка ползёт ВПЕРЁД по пути: голова path[len-1] -> path[len].
    // В гамильтоновом «змеевидном» пути соседние шаги всегда перпендикулярны
    // или прямые, разворот на 180° не требуется.
    g->dx_ = p[len].x - p[len - 1].x;
    g->dy_ = p[len].y - p[len - 1].y;
    g->score_ = static_cast<int>(len - 3);
    g->move_interval_ = 150;
    clear_turns(g);
    g->food_ = p[len];
    if (food_out) *food_out = p[len];
}

// ---------------------------------------------------------------- 4.2.1
void test_move_each_direction() {
    struct Case { int dx, dy; const char* name; };
    const Case cases[] = {{1, 0, "right"}, {0, 1, "down"}, {-1, 0, "left"}, {0, -1, "up"}};
    for (const Case& c : cases) {
        Game g(1);
        // Тело уходит НАЗАД относительно движения, чтобы не мешать голове.
        set_body(&g, straight_path(10, 7, 3, -c.dx, -c.dy));
        g.dx_ = c.dx;
        g.dy_ = c.dy;
        g.food_ = {(c.dx > 0) ? 0 : 19, (c.dy > 0) ? 0 : 14};
        clear_turns(&g);
        StepOutcome out = g.step();
        Point want{10 + c.dx, 7 + c.dy};
        CHECK(out.kind == StepKind::Moved, std::string("move ") + c.name + ": kind Moved");
        CHECK(out.head == want, std::string("move ") + c.name + ": head moved by 1 cell");
        CHECK(g.snake().size() == 3, std::string("move ") + c.name + ": length unchanged");
        CHECK(g.dir_x() == c.dx && g.dir_y() == c.dy, std::string("move ") + c.name + ": direction kept");
    }
}

// ---------------------------------------------------------------- 4.2.2
void test_wall_collisions() {
    struct Case { Point head; int dx, dy; const char* name; };
    const Case cases[] = {
        {{19, 7}, 1, 0, "right wall"},
        {{0, 7}, -1, 0, "left wall"},
        {{10, 14}, 0, 1, "bottom wall"},
        {{10, 0}, 0, -1, "top wall"},
    };
    for (const Case& c : cases) {
        Game g(1);
        set_body(&g, straight_path(c.head.x, c.head.y, 3, -c.dx, -c.dy));
        g.dx_ = c.dx;
        g.dy_ = c.dy;
        g.food_ = {(c.head.x + 5) % 20, (c.head.y + 7) % 15};
        clear_turns(&g);
        size_t len_before = g.snake().size();
        StepOutcome out = g.step();
        CHECK(out.kind == StepKind::WallCollision, std::string("wall ") + c.name + ": WallCollision");
        CHECK(g.snake().size() == len_before, std::string("wall ") + c.name + ": body not extended");
        CHECK(out.head.x < 0 || out.head.x >= 20 || out.head.y < 0 || out.head.y >= 15,
              std::string("wall ") + c.name + ": reported head is out of bounds");
    }
    // На 1 клетку внутри границы — шаг в стену допустим, поле цело.
    {
        Game g(2);
        set_body(&g, straight_path(18, 7, 3, -1, 0));
        g.dx_ = 1;
        g.dy_ = 0;
        g.food_ = {5, 5};
        clear_turns(&g);
        StepOutcome out = g.step();
        CHECK(out.kind == StepKind::Moved, "one cell inside the wall: still moves");
        CHECK(out.head == Point{19, 7}, "one cell inside the wall: head at last column");
    }
    {
        Game g2(3);
        set_body(&g2, straight_path(10, 1, 3, 0, 1));
        g2.dx_ = 0;
        g2.dy_ = -1;
        g2.food_ = {5, 12};
        clear_turns(&g2);
        StepOutcome out2 = g2.step();
        CHECK(out2.kind == StepKind::Moved, "one cell inside the top wall: still moves");
        CHECK(out2.head == Point{10, 0}, "one cell inside the top wall: head at row 0");
    }
}

// ---------------------------------------------------------------- 4.2.3
void test_self_collision_and_tail_cell() {
    // Голова входит в клетку шеи (не в хвост).
    {
        Game g(4);
        set_body(&g, {{5, 5}, {5, 4}, {6, 4}, {6, 5}});
        g.dx_ = 0;
        g.dy_ = -1;
        g.food_ = {0, 0};
        clear_turns(&g);
        StepOutcome out = g.step();
        CHECK(out.kind == StepKind::SelfCollision, "head into neck cell: SelfCollision");
        CHECK(g.snake().size() == 4, "self collision: body not extended");
    }
    // Голова входит в клетку хвоста, еды нет: по решению пользователя разрешено.
    {
        Game t(5);
        set_body(&t, {{5, 5}, {4, 5}, {4, 6}, {5, 6}, {6, 6}, {6, 5}});
        t.dx_ = 1;
        t.dy_ = 0;
        t.food_ = {0, 0};
        clear_turns(&t);
        size_t len_before = t.snake().size();
        StepOutcome tout = t.step();
        CHECK(tout.kind == StepKind::Moved, "stepping into the vacating tail cell is allowed");
        CHECK(t.snake().size() == len_before, "tail cell step keeps the length");
    }
    // Та же клетка хвоста, но на этом шаге змейка растёт: хвост не освобождается.
    {
        Game g2(6);
        set_body(&g2, {{5, 5}, {4, 5}, {4, 6}, {5, 6}, {6, 6}, {6, 5}});
        g2.dx_ = 1;
        g2.dy_ = 0;
        g2.food_ = {6, 5};
        clear_turns(&g2);
        StepOutcome out2 = g2.step();
        CHECK(out2.kind == StepKind::SelfCollision, "tail cell occupied by growth: SelfCollision");
    }
    // Рост в клетку хвоста без еды — невозможная комбинация, оставляем как есть.
}

// ---------------------------------------------------------------- 4.2.4
void test_turns() {
    {
        Game g(7);
        set_body(&g, straight_path(10, 7, 3, -1, 0));
        g.dx_ = 1;
        g.dy_ = 0;
        g.food_ = {0, 14};
        clear_turns(&g);
        g.queue_turn(-1, 0);
        CHECK(g.pending_turns_.empty(), "180 turn blocked: queue stays empty");
        StepOutcome out = g.step();
        CHECK(out.head == Point{11, 7}, "180 turn blocked: still moves right");
    }
    // Два быстрых поворота за один тик (вправо -> вверх -> влево).
    {
        Game q(8);
        set_body(&q, straight_path(10, 7, 3, -1, 0));
        q.dx_ = 1;
        q.dy_ = 0;
        q.food_ = {19, 14};
        clear_turns(&q);
        q.queue_turn(0, -1);
        q.queue_turn(-1, 0);
        CHECK(q.pending_turns_.size() == 2, "two fast turns are queued");
        StepOutcome s1 = q.step();
        CHECK(s1.head == Point{10, 6}, "first queued turn applied on tick 1");
        StepOutcome s2 = q.step();
        CHECK(s2.head == Point{9, 6}, "second queued turn applied on tick 2");
    }
    // Связка, невозможная за два тика (вправо -> вверх -> вниз).
    {
        Game r(9);
        set_body(&r, straight_path(10, 7, 3, -1, 0));
        r.dx_ = 1;
        r.dy_ = 0;
        r.food_ = {19, 14};
        clear_turns(&r);
        r.queue_turn(0, -1);
        r.queue_turn(0, 1);
        CHECK(r.pending_turns_.size() == 1, "impossible up+down pair: second turn rejected");
    }
    // Заполнение очереди «змейкой» направлений: вправо -> вверх -> влево -> вниз.
    {
        Game f(10);
        set_body(&f, straight_path(10, 7, 3, -1, 0));
        f.dx_ = 1;
        f.dy_ = 0;
        clear_turns(&f);
        f.queue_turn(0, -1);
        f.queue_turn(-1, 0);
        f.queue_turn(0, 1);
        size_t full = f.pending_turns_.size();
        f.queue_turn(1, 0);   // уже нет места и это разворот относительно «вниз»
        CHECK(full == 2, "two turns fit the queue (got " + std::to_string(full) + ")");
        CHECK(f.pending_turns_.size() == 2, "queue is capped at 2, 3rd turn dropped");
        // Миллион одинаковых нажатий не должен ни упасть, ни съесть память.
        for (int i = 0; i < 1000000; ++i) f.queue_turn(1, 0);
        CHECK(f.pending_turns_.size() == 2, "1e6 repeats keep the queue at 2 (got " +
                                                 std::to_string(f.pending_turns_.size()) + ")");
        // Очередь из 2 поворотов применяется за 2 тика без «проваливания».
        while (!f.pending_turns_.empty()) f.step();
        CHECK(g_empty(f), "queued turns drained one per tick");
    }
    // Разворот относительно НАПРАВЛЕНИЯ В ОЧЕРЕДИ, а не текущего.
    {
        Game x(17);
        set_body(&x, straight_path(10, 7, 3, -1, 0));
        x.dx_ = 1;
        x.dy_ = 0;
        clear_turns(&x);
        x.queue_turn(0, -1);   // вверх
        x.queue_turn(0, 1);    // вниз — разворот относительно "вверх", должен быть отброшен
        CHECK(x.pending_turns_.size() == 1, "turn against the queued direction is rejected");
    }
    // Повтор того же направления не дублируется.
    {
        Game d(11);
        set_body(&d, straight_path(10, 7, 3, -1, 0));
        d.dx_ = 1;
        d.dy_ = 0;
        clear_turns(&d);
        d.queue_turn(1, 0);
        d.queue_turn(1, 0);
        CHECK(d.pending_turns_.empty(), "same direction is not queued twice");
    }
}

// ---------------------------------------------------------------- 4.2.5
void test_growth() {
    {
        Game g(12);
        set_body(&g, straight_path(5, 5, 3, -1, 0));
        g.dx_ = 1;
        g.dy_ = 0;
        g.score_ = 0;
        g.move_interval_ = 150;
        clear_turns(&g);
        g.food_ = {6, 5};
        StepOutcome out = g.step();
        CHECK(out.kind == StepKind::AteFood, "eating: AteFood");
        CHECK(out.score == 0, "score in outcome is the score BEFORE the step");
        CHECK(g.score() == 1, "score incremented to 1");
        CHECK(g.snake().size() == 4, "length grew by 1");
    }
    // Рост на 1 после 10 и 100 съеденных.
    for (int target : {10, 100}) {
        Game h(13);
        int eaten = 0;
        for (int i = 0; i < target; ++i) {
            set_along_path(&h, static_cast<size_t>(3 + i), nullptr);
            StepOutcome o = h.step();
            if (o.kind != StepKind::AteFood) {
                CHECK(false, "growth loop for " + std::to_string(target) + " broke at food " +
                                 std::to_string(i));
                break;
            }
            ++eaten;
        }
        CHECK(eaten == target, "ate " + std::to_string(target) + " foods in a row");
        CHECK(h.snake().size() == static_cast<size_t>(3 + target),
              "after " + std::to_string(target) + " foods length is " + std::to_string(3 + target));
        CHECK(h.score() == target, "score after " + std::to_string(target) + " foods");
    }
    // 296 съеденных = длина 299, ещё одна клетка свободна.
    {
        Game w(14);
        set_along_path(&w, 299, nullptr);
        StepOutcome o = w.step();
        CHECK(w.snake().size() == 300, "length 300 after eating with length 299");
        CHECK(o.kind == StepKind::Won, "last free cell reports Won (got " +
                                          std::to_string(static_cast<int>(o.kind)) + ")");
        CHECK(w.score() == 297, "winning score is 297 (got " + std::to_string(w.score()) + ")");
    }
    // Победа не наступила, но длина 299 и ход дальше — обычный AteFood.
    {
        Game w2(15);
        set_along_path(&w2, 298, nullptr);
        StepOutcome o = w2.step();
        CHECK(o.kind == StepKind::AteFood, "length 299 is not a win yet");
        CHECK(w2.snake().size() == 299, "length 299 reached");
    }
}

// ---------------------------------------------------------------- 4.2.6
void test_food_spawn() {
    int inside = 0;
    int out_of_field = 0;
    for (int i = 0; i < 10000; ++i) {
        Game g(static_cast<uint32_t>(i));
        int x0 = i % 18;
        int y0 = (i / 18) % 14;
        set_body(&g, {{x0, y0}, {x0 + 1, y0}});
        g.place_food();
        for (const Point& p : g.snake())
            if (p == g.food()) ++inside;
        if (g.food().x < 0 || g.food().x >= 20 || g.food().y < 0 || g.food().y >= 15) ++out_of_field;
    }
    CHECK(inside == 0, "food never spawns inside the snake (10000 spawns)");
    CHECK(out_of_field == 0, "food always inside the field bounds");

    // Случай одной свободной клетки.
    {
        Game g(99);
        std::vector<Point> cells;
        for (int y = 0; y < 15; ++y)
            for (int x = 0; x < 20; ++x) cells.push_back({x, y});
        cells.pop_back();  // свободна только (19,14)
        set_body(&g, cells);
        g.place_food();
        CHECK(g.food() == Point{19, 14}, "food goes to the only free cell");
    }
    // Случай 0 свободных клеток: place_food обязан выйти, поле не залипает.
    {
        Game g(100);
        std::vector<Point> cells;
        for (int y = 0; y < 15; ++y)
            for (int x = 0; x < 20; ++x) cells.push_back({x, y});
        set_body(&g, cells);
        Point before = g.food();
        g.place_food();
        CHECK(g.food() == before, "place_food with a full field leaves food untouched");
        CHECK(true, "place_food with a full field returns without hanging");
    }
}

// ---------------------------------------------------------------- 4.2.7
void test_reset() {
    Game g(15);
    set_body(&g, {{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}});
    g.dx_ = 0;
    g.dy_ = 1;
    g.score_ = 42;
    g.move_interval_ = 60;
    g.queue_turn(1, 0);
    g.queue_turn(0, -1);
    g.reset();
    CHECK(g.score() == 0, "reset: score back to 0");
    CHECK(g.move_interval() == 150, "reset: move interval back to 150");
    CHECK(g.dir_x() == 1 && g.dir_y() == 0, "reset: direction back to right");
    CHECK(g.snake().size() == 3, "reset: length back to 3");
    CHECK(g.pending_turns_.empty(), "reset: turn queue cleared");
    CHECK(g.snake().front() == Point{10, 7}, "reset: head back to the centre");
    CHECK(g.snake()[2] == Point{8, 7}, "reset: tail back to the centre-2");
    bool food_inside = false;
    for (const Point& p : g.snake())
        if (p == g.food()) food_inside = true;
    CHECK(!food_inside, "reset: food outside the snake");
}

// ---------------------------------------------------------------- 4.2.8
void test_speed_bounds() {
    Game g(16);
    uint32_t prev = 150;
    bool monotonic = true;
    bool positive = true;
    int eaten = 0;
    for (int i = 0; i < 400; ++i) {
        set_along_path(&g, static_cast<size_t>(3 + i), nullptr);
        StepOutcome o = g.step();
        if (o.kind == StepKind::Won) {
            ++eaten;
            break;
        }
        if (o.kind != StepKind::AteFood) break;
        ++eaten;
        uint32_t iv = g.move_interval();
        if (iv == 0) positive = false;
        if (iv > prev) monotonic = false;
        prev = iv;
    }
    CHECK(positive, "move interval never becomes 0");
    CHECK(monotonic, "move interval never grows with score");
    CHECK(eaten == 297, "ate all 297 foods along the Hamiltonian path (got " +
                            std::to_string(eaten) + ")");
    CHECK(prev == 60, "move interval floors at 60 (got " + std::to_string(prev) + ")");
}

// ---------------------------------------------------------------- 4.2.9
void test_score_type() {
    // Экстремальный счёт: переполнение int подтверждено UBSan (см. game.cpp:83).
    Game g(17);
    set_body(&g, straight_path(5, 5, 3, -1, 0));
    g.score_ = 2147483647;
    g.food_ = {6, 5};
    clear_turns(&g);
    g.step();
    CHECK(g.score() < 0, "score overflow wraps to negative (documented, not fixed)");
}

// ---------------------------------------------------------------- 4.2.10
void test_determinism() {
    Game a(4242);
    Game b(4242);
    CHECK(a.food() == b.food(), "same seed -> same first food");
    bool same = true;
    for (int i = 0; i < 200; ++i) {
        StepOutcome sa = a.step();
        StepOutcome sb = b.step();
        if (!(sa.head == sb.head) || a.score() != b.score() || !(a.food() == b.food())) {
            same = false;
            break;
        }
    }
    CHECK(same, "same seed -> identical game after 200 steps");

    Game c(4242);
    c.reset();
    Game d(4242);
    d.reset();
    CHECK(c.food() == d.food(), "reset with the same seed -> same food (rng not reseeded)");
}

// ---------------------------------------------------------------- 4.2.11
void test_win_by_filling_field() {
    Game g(777);
    bool won = false;
    for (int i = 0; i < 400; ++i) {
        set_along_path(&g, static_cast<size_t>(3 + i), nullptr);
        StepOutcome o = g.step();
        if (o.kind == StepKind::Won) {
            won = true;
            break;
        }
        if (o.kind != StepKind::AteFood) {
            CHECK(false, std::string("filling the field died: kind=") +
                             std::to_string(static_cast<int>(o.kind)));
            return;
        }
    }
    CHECK(won, "filling the whole field reports StepKind::Won");
    CHECK(g.snake().size() == 300, "winning snake occupies all 300 cells (got " +
                                       std::to_string(g.snake().size()) + ")");
    CHECK(g.score() == 297, "winning score is 297 (got " + std::to_string(g.score()) + ")");
    // После победы place_food() на заполненном поле не должен зависать.
    g.place_food();
    CHECK(true, "place_food after the win returns without hanging");
}

// ---------------------------------------------------------------- 4.2.12
// Регресс: съеденное яблоко обязано появляться заново. Раньше food_ оставался на
// клетке, которую только что заняла голова: яблоко оставалось видно, но лежало
// внутри змейки и больше никогда не съедалось, а счёт рос только на 1.
void test_food_respawn_after_eating() {
    auto food_inside = [](const Game& g) {
        for (const Point& p : g.snake())
            if (p == g.food()) return true;
        return false;
    };

    // Первое яблоко ставим прямо перед головой: после шага новая еда обязана быть
    // другой клеткой, иначе яблоко навсегда осталось бы под змейкой.
    {
        Game g(500);
        set_body(&g, straight_path(5, 5, 3, -1, 0));
        g.dx_ = 1;
        g.dy_ = 0;
        g.score_ = 0;
        g.move_interval_ = 150;
        clear_turns(&g);
        g.food_ = {6, 5};
        Point eaten = g.food();
        StepOutcome out = g.step();
        CHECK(out.kind == StepKind::AteFood, "respawn: the first apple is eaten");
        CHECK(g.score() == 1, "respawn: score is 1 after the first apple");
        CHECK(g.snake().size() == 4, "respawn: length grew by 1");
        CHECK(!(g.food() == eaten), "respawn: the new apple is not on the eaten cell");
        CHECK(!food_inside(g), "respawn: the new apple is not inside the snake");
    }

    // Дальше еду руками не подставляем: гоним настоящее яблоко жадно и проверяем,
    // что счёт растёт много раз подряд и еда всегда вне змейки.
    {
        Game g(501);
        int eaten = 0;
        int ticks = 0;
        bool stuck = false;
        while (eaten < 5 && ticks < 5000) {
            ++ticks;
            int cx = g.snake().front().x;
            int cy = g.snake().front().y;
            // Только вперёд и вбок: разворот на 180° запрещён.
            const int cand[3][2] = {{g.dx_, g.dy_}, {-g.dy_, g.dx_}, {g.dy_, -g.dx_}};
            bool moved = false;
            for (const auto& d : cand) {
                Point np{cx + d[0], cy + d[1]};
                if (np.x < 0 || np.x >= 20 || np.y < 0 || np.y >= 15) continue;
                // В клетку еды входим всегда, в хвост — только если он успевает
                // уйти (роста на этом шаге не будет, т.к. это не еда).
                if (!(np == g.food()) && np == g.snake().back()) continue;
                bool in_body = false;
                for (size_t i = 1; i + 1 < g.snake().size(); ++i)
                    if (g.snake()[i] == np) in_body = true;
                if (in_body) continue;
                // Из допустимых берём шаг, уменьшающий расстояние до яблока.
                int before = std::abs(cx - g.food().x) + std::abs(cy - g.food().y);
                int after = std::abs(np.x - g.food().x) + std::abs(np.y - g.food().y);
                if (moved && after >= before) continue;
                moved = true;
                g.dx_ = d[0];
                g.dy_ = d[1];
            }
            if (!moved) {
                stuck = true;
                break;
            }
            StepOutcome o = g.step();
            if (o.kind != StepKind::AteFood && o.kind != StepKind::Moved) {
                CHECK(false, "respawn: greedy snake died with kind=" +
                                 std::to_string(static_cast<int>(o.kind)));
                return;
            }
            if (o.kind == StepKind::AteFood) {
                ++eaten;
                if (food_inside(g)) {
                    CHECK(false, "respawn: apple spawned inside the snake on tick " +
                                     std::to_string(ticks));
                    return;
                }
            }
        }
        CHECK(!stuck, "respawn: greedy snake never got stuck");
        CHECK(eaten == 5, "respawn: ate 5 apples in a row with no food placed by hand (got " +
                              std::to_string(eaten) + " in " + std::to_string(ticks) + " ticks)");
        CHECK(g.score() == 5, "respawn: score reached 5 (got " + std::to_string(g.score()) + ")");
        CHECK(g.snake().size() == 8, "respawn: length grew to 8 (got " +
                                         std::to_string(g.snake().size()) + ")");
    }
}

}  // namespace

int main() {
    test_move_each_direction();
    test_wall_collisions();
    test_self_collision_and_tail_cell();
    test_turns();
    test_growth();
    test_food_spawn();
    test_reset();
    test_speed_bounds();
    test_score_type();
    test_determinism();
    test_win_by_filling_field();
    test_food_respawn_after_eating();

    std::printf("\nunit_game: pass=%d fail=%d\n", g_pass, g_fail);
    for (const std::string& s : g_failed) std::printf("  failed: %s\n", s.c_str());
    return g_fail == 0 ? 0 : 1;
}