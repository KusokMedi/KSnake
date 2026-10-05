// Юнит-тесты проверки обновлений KSnake (update.cpp).
// Сетевой слой подменяется stub-ом curl на PATH, поэтому тесты ходят по
// настоящему коду http_get -> файла -> JSON -> состояние.
// Сборка: ./tests/build_unit.sh
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <SDL.h>

#define private public
#include "../src/update.h"
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

constexpr uint64_t AUTO_MS = 8ull * 60 * 60 * 1000;   // AUTO_UPDATE_PERIOD_MS
constexpr uint64_t TTL_MS = 5ull * 60 * 1000;         // UPDATE_CACHE_TTL_MS

std::string g_pref = "/tmp/ks_upd_pref/";

void write_file(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary);
    f << body;
}
std::string read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Кладёт на PATH stub curl: он получает -o <путь> и пишет туда $KS_STUB_BODY
// (или ничего и возвращает код $KS_STUB_RC).
void install_stub_curl(const std::string& dir) {
    std::string s;
    s += "#!/bin/sh\n";
    s += "out=\"\"\n";
    s += "prev=\"\"\n";
    s += "for a in \"$@\"; do\n";
    s += "  if [ \"$prev\" = \"-o\" ]; then out=\"$a\"; fi\n";
    s += "  prev=\"$a\"\n";
    s += "done\n";
    s += "if [ -n \"$KS_STUB_SLEEP\" ]; then sleep \"$KS_STUB_SLEEP\"; fi\n";
    s += "if [ -n \"$KS_STUB_B64\" ]; then printf '%s' \"$KS_STUB_B64\" | base64 -d > \"$out\" 2>/dev/null; fi\n";
    s += "exit ${KS_STUB_RC:-0}\n";
    write_file(dir + "/curl", s);
    std::string chmod_cmd = "chmod +x '" + dir + "/curl'";
    if (std::system(chmod_cmd.c_str()) != 0) check(false, "chmod stub curl");
}

void reset_pref() {
    std::string cmd = "rm -rf '" + g_pref + "' && mkdir -p '" + g_pref + "'";
    std::system(cmd.c_str());
}

// base64 тела ответа: stub curl получает строку без кавычек и переводов строк.
std::string b64(const std::string& s) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    while (i + 2 < s.size()) {
        unsigned v = (static_cast<unsigned char>(s[i]) << 16) |
                     (static_cast<unsigned char>(s[i + 1]) << 8) |
                     static_cast<unsigned char>(s[i + 2]);
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += tbl[v & 63];
        i += 3;
    }
    if (i + 1 == s.size()) {
        unsigned v = static_cast<unsigned char>(s[i]) << 16;
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == s.size()) {
        unsigned v = (static_cast<unsigned char>(s[i]) << 16) |
                     (static_cast<unsigned char>(s[i + 1]) << 8);
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += "=";
    }
    return out;
}

// ---------------------------------------------------------------- 4.4.1
void test_restore_cached() {
    struct Case { const char* cache; int local[3]; UpdateState want; const char* what; };
    const Case cases[] = {
        {"1700000000 4 2.0 https://e\n", {1, 1, 0}, UpdateState::Available,
         "cached 2.0 vs local 1.1 -> Available"},
        {"1700000000 4 0.9 https://e\n", {1, 1, 0}, UpdateState::Ahead,
         "cached 0.9 vs local 1.1 -> Ahead"},
        {"1700000000 4 1.1 https://e\n", {1, 1, 0}, UpdateState::UpToDate,
         "cached 1.1 vs local 1.1 -> UpToDate"},
        {"1700000000 4 - https://e\n", {1, 1, 0}, UpdateState::Unknown,
         "cached state Unknown survives when the version is unparsable"},
        {"1700000000 5 - \n", {1, 1, 0}, UpdateState::Idle,
         "cached Failed with no version -> stays Idle (no stale Failed)"},
        {"1700000000 2 - \n", {1, 1, 0}, UpdateState::Idle,
         "cached Checking with no version -> stays Idle"},
        {"", {1, 1, 0}, UpdateState::Idle, "empty cache -> Idle"},
        {"garbage\n", {1, 1, 0}, UpdateState::Idle, "garbage cache -> Idle"},
        // Битое поле состояния больше не обнуляет кэш: состояние всё равно
        // пересчитывается через compare_versions, поэтому версия 2.0 доходит
        // до игрока, а состояние определяется сравнением с локальной.
        {"1700000000 abc 2.0 \n", {1, 1, 0}, UpdateState::Available,
         "corrupt state field does not discard the cached version"},
    };
    for (const Case& c : cases) {
        reset_pref();
        write_file(g_pref + "update_check.txt", c.cache);
        UpdateChecker u(g_pref, std::to_string(c.local[0]) + "." + std::to_string(c.local[1]));
        u.restore_cached();
        CHECK(u.state() == c.want, std::string(c.what) + " (got state=" +
                                     std::to_string(static_cast<int>(u.state())) + ")");
    }
    // Кэш без файла.
    {
        reset_pref();
        UpdateChecker u(g_pref, "1.1");
        u.restore_cached();
        CHECK(u.state() == UpdateState::Idle, "no cache file -> Idle");
    }
    // URL из кэша сохраняется (он нужен кнопке «Open release page»).
    {
        reset_pref();
        write_file(g_pref + "update_check.txt", "1700000000 4 2.0 https://example.com/r\n");
        UpdateChecker u(g_pref, "1.1");
        u.restore_cached();
        CHECK(u.url() == "https://example.com/r", "cached URL is restored (got \"" + u.url() + "\")");
        CHECK(u.version() == "2.0", "cached version is formatted (got \"" + u.version() + "\")");
    }
    // Подпись состояния всегда пересчитывается: сохранённый Available при
    // локальной версии 9.0 должен стать Ahead.
    {
        reset_pref();
        write_file(g_pref + "update_check.txt", "1700000000 4 2.0 \n");
        UpdateChecker u(g_pref, "9.0");
        u.restore_cached();
        CHECK(u.state() == UpdateState::Ahead,
              "cached Available is recomputed for a newer local build (got state=" +
                  std::to_string(static_cast<int>(u.state())) + ")");
    }
}

// ---------------------------------------------------------------- 4.4.2
void test_result_is_fresh() {
    UpdateChecker u(g_pref, "1.1");
    CHECK(!u.result_is_fresh(1000), "no result yet -> not fresh");
    u.set_result(UpdateState::UpToDate, "1.1", "");
    CHECK(!u.result_is_fresh(1000), "set_result does not stamp result_ms_ -> not fresh");

    // Границы TTL ровно и ±1 мс.
    struct T { uint64_t now; bool want; const char* what; };
    const T ts[] = {
        {TTL_MS - 1, true, "TTL - 1 ms is fresh"},
        {TTL_MS, false, "exactly TTL is stale"},
        {TTL_MS + 1, false, "TTL + 1 ms is stale"},
    };
    for (const T& t : ts) {
        UpdateChecker v(g_pref, "1.1");
        v.result_ms_ = 1000;
        v.state_ = UpdateState::UpToDate;
        CHECK(v.result_is_fresh(1000 + t.now) == t.want, t.what);
    }
    // Checking всегда «не свежий», даже если результат недавний.
    {
        UpdateChecker v(g_pref, "1.1");
        v.result_ms_ = 1000;
        v.state_ = UpdateState::Checking;
        CHECK(!v.result_is_fresh(1001), "Checking is never fresh");
    }
}

// ---------------------------------------------------------------- 4.4.3
void test_background_cycle() {
    // Граница 8 часов: start_background_cycle оперирует epoch-секундами, поэтому
    // проверяем через background_due/schedule_next_background в миллисекундах.
    {
        UpdateChecker u(g_pref, "1.1");
        u.schedule_next_background(1000);
        CHECK(!u.background_due(1000), "not due at the scheduled moment");
        CHECK(!u.background_due(1000 + AUTO_MS - 1), "not due 1 ms before the period");
        CHECK(u.background_due(1000 + AUTO_MS), "due exactly at AUTO_UPDATE_PERIOD_MS");
        CHECK(u.background_due(1000 + AUTO_MS + 1), "due 1 ms after the period");
    }
    // Стартовый цикл: свежий кэш откладывает проверку, старый — запускает.
    reset_pref();
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(std::time(nullptr)));
        write_file(g_pref + "update_check.txt", std::string(buf) + " 4 1.1 \n");
        UpdateChecker u(g_pref, "1.1");
        u.start_background_cycle(0);
        CHECK(u.state_ == UpdateState::Idle,
              "fresh cache: the startup check is deferred, state stays Idle (got state=" +
                  std::to_string(static_cast<int>(u.state())) + ")");
    }
    reset_pref();
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%llu",
                      static_cast<unsigned long long>(std::time(nullptr) - AUTO_MS / 1000 - 60));
        write_file(g_pref + "update_check.txt", std::string(buf) + " 4 1.1 \n");
        UpdateChecker u(g_pref, "1.1");
        u.start_background_cycle(0);
        CHECK(u.state() == UpdateState::Checking,
              "cache older than 8 h: the startup check runs");
    }
    // Перевод часов назад: unsigned-арифметика даёт корректную задержку.
    for (uint64_t back : {0ull, 3600ull, 86400ull, 8640000ull}) {
        reset_pref();
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%llu",
                      static_cast<unsigned long long>(std::time(nullptr) + back));
        write_file(g_pref + "update_check.txt", std::string(buf) + " 4 1.1 \n");
        UpdateChecker u(g_pref, "1.1");
        u.start_background_cycle(0);
        bool deferred = !u.background_due(0);
        CHECK(deferred, "clock moved back " + std::to_string(back) +
                            " s: check deferred, not fired immediately");
        uint64_t left_ms = u.next_auto_ms_;
        CHECK(left_ms >= 8ull * 3600 * 1000,
              "clock moved back " + std::to_string(back) +
                  " s: delay is not shorter than 8 h (got " + std::to_string(left_ms / 3600000) +
                  " h)");
    }
    // Граница ровно 8 часов в epoch-секундах.
    reset_pref();
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%llu",
                      static_cast<unsigned long long>(std::time(nullptr) - AUTO_MS / 1000));
        write_file(g_pref + "update_check.txt", std::string(buf) + " 4 1.1 \n");
        UpdateChecker u(g_pref, "1.1");
        u.start_background_cycle(0);
        CHECK(u.state() == UpdateState::Checking,
              "cache exactly 8 h old: the check runs (boundary is '>='), got state=" +
                  std::to_string(static_cast<int>(u.state())));
    }
    // Часы переведены назад: last > now.
    reset_pref();
    {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%llu",
                      static_cast<unsigned long long>(std::time(nullptr) + 86400));
        write_file(g_pref + "update_check.txt", std::string(buf) + " 4 1.1 \n");
        UpdateChecker u(g_pref, "1.1");
        u.start_background_cycle(0);
        // now_epoch - last валится в uint64 -> next_auto_ms_ тоже.
        bool due_now = u.background_due(0);
        std::printf("   clock moved back 1 day -> background_due(0) = %s\n",
                    due_now ? "true" : "false");
        CHECK(!due_now, "clock moved back: next background check is not scheduled into the past");
    }
}

// ---------------------------------------------------------------- 4.4.4
void test_poll_transitions() {
    const std::string stub_dir = "/tmp/ks_upd_stub/";
    std::string cmd = "rm -rf '" + stub_dir + "' && mkdir -p '" + stub_dir + "'";
    std::system(cmd.c_str());
    install_stub_curl(stub_dir);
    std::string old_path = getenv("PATH") ? getenv("PATH") : "";
    setenv("PATH", (stub_dir + ":" + old_path).c_str(), 1);

    auto run_case = [&](const std::string& body, const std::string& local, UpdateState want,
                        const char* what) {
        reset_pref();
        setenv("KS_STUB_B64", b64(body).c_str(), 1);
        setenv("KS_STUB_RC", "0", 1);
        unsetenv("KS_STUB_SLEEP");
        UpdateChecker u(g_pref, local);
        u.start(false);
        for (int i = 0; i < 200; ++i) {
            u.poll();
            if (u.state() != UpdateState::Checking) break;
            SDL_Delay(20);
        }
        CHECK(u.state() == want, std::string(what) + " (got state=" +
                                   std::to_string(static_cast<int>(u.state())) + ", note=\"" +
                                   u.note() + "\")");
    };

    run_case("{\"tag_name\":\"v2.0.0\",\"html_url\":\"https://example.com/r\"}", "1.1",
             UpdateState::Available, "remote 2.0 vs local 1.1 -> Available");
    run_case("{\"tag_name\":\"v0.9.0\"}", "1.1", UpdateState::Ahead,
             "remote 0.9 vs local 1.1 -> Ahead");
    run_case("{\"tag_name\":\"v1.1.0\"}", "1.1", UpdateState::UpToDate,
             "remote 1.1 vs local 1.1 -> UpToDate");
    run_case("{\"message\":\"API rate limit exceeded for 1.2.3.4\"}", "1.1", UpdateState::Failed,
             "GitHub rate-limit message -> Failed");
    run_case("{\"message\":\"\xe2\x98\x83 snowman\"}", "1.1", UpdateState::Failed,
             "non-ASCII message -> Failed");
    run_case("{\"name\":\"not a version at all\"}", "1.1", UpdateState::Unknown,
             "no parsable version -> Unknown");
    run_case("{\"assets\":[{\"name\":\"KSnake-1.5-x86_64.AppImage\"}]}", "1.1",
             UpdateState::Available, "version taken from the asset name");
    run_case("", "1.1", UpdateState::Failed, "empty body -> Failed");
    run_case("{not json", "1.1", UpdateState::Failed, "broken JSON -> Failed");
    run_case("[]", "1.1", UpdateState::Unknown, "JSON array -> Unknown");
    run_case("{\"tag_name\":123}", "1.1", UpdateState::Unknown, "numeric tag_name -> Unknown");
    run_case("{\"tag_name\":\"" + std::string(60000, '9') + "\"}", "1.1", UpdateState::Available,
             "60000-digit tag_name is handled without hanging");
    // 60000 цифр насыщаются до 999999 -> 999999.0.0 новее 1.1.
    run_case("{\"tag_name\":\"" + std::string(60000, '9') + "\",\"html_url\":\"https://a.b/c\"}",
             "1.1", UpdateState::Available, "huge tag_name saturates to 999999 and compares");
    // UTF-8 BOM (curl на Windows может его вернуть).
    run_case("\xef\xbb\xbf{\"tag_name\":\"v3.0.0\"}", "1.1", UpdateState::Available,
             "UTF-8 BOM is stripped");
    // url из ответа сохраняется.
    {
        reset_pref();
        setenv("KS_STUB_B64", b64("{\"tag_name\":\"v2.0.0\",\"html_url\":\"https://ex.com/r\"}").c_str(), 1);
        UpdateChecker u(g_pref, "1.1");
        u.start(false);
        for (int i = 0; i < 200 && u.state() == UpdateState::Checking; ++i) {
            u.poll();
            SDL_Delay(20);
        }
        CHECK(u.url() == "https://ex.com/r", "html_url is captured (got \"" + u.url() + "\")");
        CHECK(u.tag() == "v2.0.0", "tag_name is captured (got \"" + u.tag() + "\")");
        std::string cache = read_file(g_pref + "update_check.txt");
        CHECK(cache.find(" 2.0 ") != std::string::npos,
              "result is written to update_check.txt in the formatted form (got \"" + cache + "\")");
        CHECK(cache.find("Available") == std::string::npos,
              "the cache stores the numeric state, not its name");
    }
    // Нет curl в PATH -> Failed, а не падение.
    {
        reset_pref();
        setenv("PATH", "/nonexistent-dir", 1);
        UpdateChecker u(g_pref, "1.1");
        u.start(false);
        for (int i = 0; i < 200 && u.state() == UpdateState::Checking; ++i) {
            u.poll();
            SDL_Delay(20);
        }
        CHECK(u.state() == UpdateState::Failed, "no curl in PATH -> Failed (got state=" +
                                                    std::to_string(static_cast<int>(u.state())) + ")");
        CHECK(read_file(g_pref + "update_check.txt").empty(),
              "Failed result is not cached (cache stays absent)");
    }
    setenv("PATH", old_path.c_str(), 1);

    // Watchdog: ответ зависает дольше 25 с -> Failed и потеря fetch_.
    {
        reset_pref();
        std::string saved = getenv("PATH") ? getenv("PATH") : "";
        setenv("PATH", (stub_dir + ":" + saved).c_str(), 1);
        setenv("KS_STUB_SLEEP", "40", 1);
        unsetenv("KS_STUB_B64");
        UpdateChecker u(g_pref, "1.1");
        u.start(false);
        // started_ms_ подводим вручную: ждать 25 с в тесте не нужно.
        u.started_ms_ = SDL_GetTicks() - 26000;
        for (int i = 0; i < 50 && u.state() == UpdateState::Checking; ++i) {
            u.poll();
            SDL_Delay(10);
        }
        CHECK(u.state() == UpdateState::Failed,
              "watchdog trips after 25 s (got state=" +
                  std::to_string(static_cast<int>(u.state())) + ", note=\"" + u.note() + "\")");
        CHECK(!u.fetch_, "watchdog releases the shared fetch state");
        CHECK(u.note().find("did not respond") != std::string::npos,
              "watchdog note mentions the timeout (got \"" + u.note() + "\")");
        setenv("PATH", saved.c_str(), 1);
        unsetenv("KS_STUB_SLEEP");
    }
    // Повторный клик во время Checking: второй запрос не заводится, иначе ответ
    // первой нити терялся, а GitHub получал лишний запрос к лимиту 60/час.
    {
        reset_pref();
        std::string saved = getenv("PATH") ? getenv("PATH") : "";
        setenv("PATH", (stub_dir + ":" + saved).c_str(), 1);
        setenv("KS_STUB_SLEEP", "3", 1);
        setenv("KS_STUB_B64", b64("{\"tag_name\":\"v2.0.0\"}").c_str(), 1);
        UpdateChecker u(g_pref, "1.1");
        u.start(false);
        UpdateChecker::Fetch* first = u.fetch_.get();
        u.start(false);
        CHECK(u.state() == UpdateState::Checking, "second click during Checking keeps Checking");
        CHECK(u.fetch_.get() == first, "second click does not spawn a second fetch");
        u.started_ms_ = SDL_GetTicks() - 26000;
        u.poll();
        CHECK(u.state() == UpdateState::Failed, "watchdog still recovers the check");
        setenv("PATH", saved.c_str(), 1);
        unsetenv("KS_STUB_SLEEP");
    }
    unsetenv("KS_STUB_B64");
    unsetenv("KS_STUB_RC");
}

// ---------------------------------------------------------------- 4.4.5
void test_mark_checked() {
    // mark_checked() не должен затирать найденное обновление.
    reset_pref();
    write_file(g_pref + "update_check.txt", "1700000000 4 2.0 https://ex.com/r\n");
    {
        UpdateChecker u(g_pref, "1.1");
        u.mark_checked();
        std::string body = read_file(g_pref + "update_check.txt");
        CHECK(body.find(" 2.0 ") != std::string::npos,
              "mark_checked keeps the cached version (got \"" + body + "\")");
        CHECK(body.find("https://ex.com/r") != std::string::npos,
              "mark_checked keeps the cached URL (got \"" + body + "\")");
    }
    // Пустой кэш: mark_checked пишет прочерк, а не пустое поле.
    reset_pref();
    {
        UpdateChecker u(g_pref, "1.1");
        u.mark_checked();
        std::string body = read_file(g_pref + "update_check.txt");
        CHECK(body.find(" - ") != std::string::npos,
              "mark_checked writes '-' for a missing version (got \"" + body + "\")");
    }
    // Битый кэш: mark_checked обязан заменить его валидным.
    reset_pref();
    write_file(g_pref + "update_check.txt", "garbage\n");
    {
        UpdateChecker u(g_pref, "1.1");
        u.mark_checked();
        UpdateChecker v(g_pref, "1.1");
        v.restore_cached();
        std::string body = read_file(g_pref + "update_check.txt");
        CHECK(body.find("garbage") == std::string::npos,
              "mark_checked replaces a broken cache (got \"" + body + "\")");
        CHECK(v.state() == UpdateState::Idle, "rewritten cache with '-' restores to Idle");
    }
    // Каталог только для чтения: mark_checked не падает.
    {
        std::string ro = "/tmp/ks_upd_ro/";
        std::system(("rm -rf '" + ro + "' && mkdir -p '" + ro + "' && chmod 500 '" + ro + "'").c_str());
        UpdateChecker u(ro, "1.1");
        u.mark_checked();
        UpdateChecker v(ro, "1.1");
        v.restore_cached();
        CHECK(v.state() == UpdateState::Idle, "unwritable cache dir: no crash");
        std::system(("chmod 700 '" + ro + "' && rm -rf '" + ro + "'").c_str());
    }
}

// ---------------------------------------------------------------- 4.4.6
void test_glow() {
    UpdateChecker u(g_pref, "1.1");
    CHECK(u.glow() == 0.0f, "glow starts at 0");
    u.set_result(UpdateState::Available, "2.0", "");
    CHECK(u.glow() == 1.0f, "Available sets the glow instantly for a manual check");
    for (int i = 0; i < 200; ++i) u.animate_glow(16);
    CHECK(u.glow() == 1.0f, "glow stays at 1 while Available");
    u.set_result(UpdateState::UpToDate, "1.1", "");
    for (int i = 0; i < 200; ++i) u.animate_glow(16);
    CHECK(u.glow() == 0.0f, "glow decays to 0 when the update disappears");
    // Фоновая проверка: подсветка плавная, а не мгновенная.
    UpdateChecker a(g_pref, "1.1");
    a.set_result(UpdateState::Available, "2.0", "");
    a.automatic_ = true;
    a.glow_ = 0.0f;
    a.animate_glow(16);
    CHECK(a.glow() > 0.0f && a.glow() < 1.0f, "background check fades the glow in gradually");
}

}  // namespace

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::printf("SDL_Init failed: %s\n", SDL_GetError());
        return 2;
    }
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    test_restore_cached();
    test_result_is_fresh();
    test_background_cycle();
    test_poll_transitions();
    test_mark_checked();
    test_glow();

    std::printf("\nunit_update: pass=%d fail=%d\n", g_pass, g_fail);
    for (const std::string& s : g_failed) std::printf("  failed: %s\n", s.c_str());
    SDL_Quit();
    return g_fail == 0 ? 0 : 1;
}