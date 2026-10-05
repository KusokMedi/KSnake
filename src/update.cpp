#include "update.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iterator>
#include <thread>

#include "util.h"

namespace ks {
namespace {

using json = nlohmann::json;

constexpr const char* GITHUB_API_LATEST =
    "https://api.github.com/repos/KusokMedi/KSnake/releases/latest";
constexpr int HTTP_TIMEOUT_S = 12;
constexpr int UPDATE_WATCHDOG_MS = 25000;
// фоновая проверка обновлений: не чаще одного раза в 8 часов
constexpr uint64_t AUTO_UPDATE_PERIOD_MS = 8ull * 60 * 60 * 1000;
// результат проверки считается свежим 5 минут — ручной клик не переспрашивает
constexpr uint64_t UPDATE_CACHE_TTL_MS = 5ull * 60 * 1000;

#ifdef _WIN32
// Скрипт для PowerShell. URL, путь вывода и User-Agent приходят позиционными
// параметрами скрипта, а не склеиваются в строку команды: иначе кавычка в пути
// к данным (а он берётся из XDG_DATA_HOME или AppData) снова станет командой.
std::string ps_fetch_script() {
    return "param([string]$Url, [string]$Out, [string]$Agent)\n"
           "$ProgressPreference = 'SilentlyContinue'\n"
           "[Net.ServicePointManager]::SecurityProtocol = 'Tls12'\n"
           "try {\n"
           "  $r = Invoke-WebRequest -UseBasicParsing -TimeoutSec " +
           std::to_string(HTTP_TIMEOUT_S) +
           " -Headers @{'User-Agent' = $Agent; 'Accept' = 'application/vnd.github+json'}"
           " -Uri $Url\n"
           "  $r.Content | Out-File -Encoding utf8 -FilePath $Out\n"
           "  exit 0\n"
           "} catch { exit 1 }\n";
}
#endif

// Minimal HTTPS GET: runs curl / PowerShell without a shell and returns the body.
// The response is written to out_path because neither tool is captured in a
// portable way without extra dependencies. Arguments go as argv, never as a
// command string, so quotes and `;` in a path stay data and cannot execute.
std::string http_get(const std::string& url, const std::string& out_path,
                     const std::string& user_agent) {
    int rc = 0;
#ifdef _WIN32
    const std::string script = out_path + ".ps1";
    {
        std::ofstream f(script, std::ios::binary);
        if (!f) return std::string();
        f << ps_fetch_script();
    }
    bool ran = run_process({"powershell", "-NoProfile", "-NonInteractive",
                            "-ExecutionPolicy", "Bypass", "-File", script, url, out_path,
                            user_agent},
                           &rc);
    std::remove(script.c_str());
    if (!ran || rc != 0) return std::string();
#else
    // Никакой оболочки: кавычки и `;` в пути остаются аргументами.
    if (!run_process({"curl", "-fsSL", "--max-time", std::to_string(HTTP_TIMEOUT_S),
                      "-H", "Accept: application/vnd.github+json", "-A", user_agent,
                      "-o", out_path, url},
                     &rc) ||
        rc != 0) {
        return std::string();
    }
#endif
    std::ifstream f(out_path, std::ios::binary);
    if (!f) return std::string();
    std::string body((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (body.size() >= 3 && static_cast<uint8_t>(body[0]) == 0xEF &&
        static_cast<uint8_t>(body[1]) == 0xBB && static_cast<uint8_t>(body[2]) == 0xBF) {
        body.erase(0, 3);
    }
    return body;
}

// Запрос без токена упирается в лимит GitHub 60/час на IP, поэтому ответ
// разбираем осторожно: любое неожиданное поле трактуем как ошибку.
bool extract_version(const json& j, int remote[3]) {
    for (const char* key : {"name", "tag_name"}) {
        auto it = j.find(key);
        if (it == j.end() || !it->is_string()) continue;
        if (parse_version(it->get<std::string>(), remote)) return true;
    }
    if (j.contains("assets") && j["assets"].is_array()) {
        for (const auto& a : j["assets"]) {
            if (!a.is_object() || !a.contains("name") || !a["name"].is_string()) continue;
            if (parse_version(a["name"].get<std::string>(), remote)) return true;
        }
    }
    return false;
}

std::string format_msg(const char* fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return std::string(buf);
}

}  // namespace

UpdateChecker::UpdateChecker(std::string pref_dir, std::string app_version)
    : pref_dir_(std::move(pref_dir)), app_version_(std::move(app_version)) {}

void UpdateChecker::set_logger(LogFn log) {
    log_ = std::move(log);
}

std::string UpdateChecker::cache_path() const {
    return pref_dir_ + "update_check.txt";
}

void UpdateChecker::save_cache(uint64_t epoch, UpdateState st, const std::string& ver,
                               const std::string& url) const {
    std::ofstream f(cache_path());
    if (!f) return;
    f << epoch << ' ' << static_cast<int>(st) << ' ' << ver << ' ' << url << std::endl;
}

void UpdateChecker::set_result(UpdateState st, const std::string& ver, const std::string& url) {
    state_ = st;
    version_ = ver;
    url_ = url;
    if (st == UpdateState::Available) glow_ = glow_target_ = 1.0f;
}

void UpdateChecker::restore_cached() {
    // результат прошлой проверки восстанавливаем сразу: если игра устарела,
    // пункт меню жёлтый уже на первом кадре, даже если запрос ещё не ушёл
    std::ifstream f(cache_path());
    if (!f) return;
    uint64_t epoch = 0;
    std::string st_token;
    std::string ver;
    if (!(f >> epoch >> st_token >> ver)) return;
    // Состояние из кэша не доверяем: его всё равно пересчитывает
    // compare_versions. Поэтому токен читаем строкой и разбираем сами — битое
    // поле вроде "abc" больше не роняет разбор и не выбрасывает хорошую версию.
    int st = -1;
    if (st_token.size() == 1 && st_token[0] >= '0' && st_token[0] <= '9') {
        st = st_token[0] - '0';
    }
    std::string rest;
    std::getline(f, rest);
    std::string url = trim_str(rest);

    int remote[3] = {0, 0, 0};
    int local[3] = {0, 0, 0};
    parse_version(app_version_, local);
    if (parse_version(ver, remote)) {
        int cmp = compare_versions(remote, local);
        set_result(cmp > 0   ? UpdateState::Available
                   : cmp < 0 ? UpdateState::Ahead
                             : UpdateState::UpToDate,
                   format_version(remote), url);
        if (log_) {
            log_(format_msg("UPDATE cached state=%d remote=%s", static_cast<int>(state_),
                            version_.c_str()));
        }
    } else if (st == static_cast<int>(UpdateState::Unknown)) {
        state_ = UpdateState::Unknown;
        if (log_) log_("UPDATE cached state=Unknown");
    }
}

void UpdateChecker::mark_checked() {
    // отмечает факт проверки, сохраняя прежний результат: перезапись одного
    // только времени стирала бы информацию о найденном обновлении
    uint64_t old_epoch = 0;
    int old_st = -1;
    std::string old_ver, old_url;
    {
        std::ifstream f(cache_path());
        if (!(f >> old_epoch >> old_st >> old_ver)) old_st = -1;
        std::string rest;
        std::getline(f, rest);
        old_url = trim_str(rest);
    }
    std::ofstream f(cache_path());
    if (!f) return;
    f << static_cast<uint64_t>(std::time(nullptr)) << ' ' << old_st << ' '
      << (old_ver.empty() ? std::string("-") : old_ver) << ' ' << old_url << std::endl;
}

bool UpdateChecker::background_due(uint64_t now_ms) const {
    return now_ms >= next_auto_ms_;
}

void UpdateChecker::schedule_next_background(uint64_t now_ms) {
    next_auto_ms_ = now_ms + AUTO_UPDATE_PERIOD_MS;
}

void UpdateChecker::start_background_cycle(uint64_t now_ms) {
    std::ifstream f(cache_path());
    uint64_t last = 0;
    f >> last;

    uint64_t now_epoch = static_cast<uint64_t>(std::time(nullptr));
    if (now_epoch >= last && now_epoch - last >= AUTO_UPDATE_PERIOD_MS / 1000) {
        start(true);
        mark_checked();
        schedule_next_background(now_ms);
        if (log_) log_("UPDATE background check at startup");
    } else {
        uint64_t left = AUTO_UPDATE_PERIOD_MS / 1000 - (now_epoch - last);
        next_auto_ms_ = now_ms + left * 1000;
        if (log_) {
            log_(format_msg("UPDATE background check deferred, next in %llu min",
                            static_cast<unsigned long long>(left / 60)));
        }
    }
}

// automatic = true — фоновая проверка (экран не открывается),
// false — ручное нажатие Check for updates
void UpdateChecker::start(bool automatic) {
    // Повторный клик во время Checking заводил вторую нить и второй curl: ответ
    // первой терялся, а GitHub получал лишний запрос к лимиту 60/час.
    if (state_ == UpdateState::Checking && fetch_) {
        if (log_) log_("UPDATE check already running");
        return;
    }
    automatic_ = automatic;
    state_ = UpdateState::Checking;
    version_.clear();
    tag_.clear();
    url_.clear();
    note_.clear();
    started_ms_ = SDL_GetTicks();
    auto fs = std::make_shared<Fetch>();
    fetch_ = fs;
    std::string out_path = pref_dir_ + "update.json";
    const std::string user_agent = "KSnake/" + app_version_;
    std::thread([fs, out_path, user_agent]() {
        std::string body = http_get(GITHUB_API_LATEST, out_path, user_agent);
        std::lock_guard<std::mutex> lk(fs->mtx);
        fs->body = std::move(body);
        fs->done = true;
    }).detach();
    if (log_) log_(format_msg("UPDATE check started auto=%d", automatic ? 1 : 0));
}

void UpdateChecker::fail(const std::string& msg) {
    state_ = UpdateState::Failed;
    note_ = msg;
    if (automatic_) mark_checked();
    if (log_) log_("UPDATE failed: " + msg);
}

void UpdateChecker::poll() {
    if (state_ != UpdateState::Checking || !fetch_) return;
    bool done = false;
    {
        std::lock_guard<std::mutex> lk(fetch_->mtx);
        done = fetch_->done;
    }
    if (!done) {
        if (SDL_GetTicks() - started_ms_ > UPDATE_WATCHDOG_MS) {
            fetch_.reset();
            fail("GitHub did not respond in time");
        }
        return;
    }
    std::string body;
    {
        std::lock_guard<std::mutex> lk(fetch_->mtx);
        body = fetch_->body;
    }
    fetch_.reset();

    if (trim_str(body).empty()) {
        fail("No response from GitHub");
        return;
    }

    json j;
    try {
        j = json::parse(body);
    } catch (...) {
        fail("Unexpected answer from GitHub");
        return;
    }

    if (j.contains("message") && j["message"].is_string()) {
        std::string msg = trim_str(j["message"].get<std::string>());
        if (msg.size() > 46) msg = msg.substr(0, 43) + "...";
        fail(msg);
        return;
    }

    if (j.contains("html_url") && j["html_url"].is_string()) {
        url_ = j["html_url"].get<std::string>();
    }
    if (j.contains("tag_name") && j["tag_name"].is_string()) {
        tag_ = j["tag_name"].get<std::string>();
    }

    int remote[3] = {0, 0, 0};
    if (!extract_version(j, remote)) {
        state_ = UpdateState::Unknown;
        result_ms_ = SDL_GetTicks64();
        save_cache(static_cast<uint64_t>(std::time(nullptr)), state_, std::string(), url_);
        return;
    }

    int local[3] = {0, 0, 0};
    parse_version(app_version_, local);
    version_ = format_version(remote);
    int cmp = compare_versions(remote, local);
    state_ = cmp > 0   ? UpdateState::Available
             : cmp < 0 ? UpdateState::Ahead
                       : UpdateState::UpToDate;
    result_ms_ = SDL_GetTicks64();
    save_cache(static_cast<uint64_t>(std::time(nullptr)), state_, version_, url_);
    if (log_) {
        log_(format_msg("UPDATE state=%d remote=%s local=%s", static_cast<int>(state_),
                        version_.c_str(), format_version(local).c_str()));
    }
}

void UpdateChecker::animate_glow(uint32_t frame_delta) {
    // пункт Check for updates окрашивается, пока доступно обновление;
    // результат фоновой проверки проявляется плавно, ручной — сразу
    if (state_ == UpdateState::Available) {
        glow_target_ = 1.0f;
        if (!automatic_) glow_ = 1.0f;
    } else if (state_ != UpdateState::Checking) {
        glow_target_ = 0.0f;
    }
    if (glow_ != glow_target_) {
        float k = std::min(1.0f, static_cast<float>(frame_delta) / 300.0f);
        glow_ += (glow_target_ - glow_) * k;
        if (std::fabs(glow_ - glow_target_) < 0.004f) glow_ = glow_target_;
    }
}

bool UpdateChecker::result_is_fresh(uint64_t now_ms) const {
    return result_ms_ != 0 && state_ != UpdateState::Checking &&
           now_ms - result_ms_ < UPDATE_CACHE_TTL_MS;
}

}  // namespace ks