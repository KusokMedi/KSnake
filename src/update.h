#pragma once

#include <SDL.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace ks {

enum class UpdateState { Idle, Checking, UpToDate, Available, Unknown, Failed, Ahead };

// Проверка обновлений: фоновая с троттлингом в 8 часов плюс ручная по клику.
// Результат хранится в update_check.txt рядом с scores.json, поэтому жёлтый
// пункт меню виден сразу на первом кадре, даже если запрос ещё не ушёл.
class UpdateChecker {
public:
    // Сообщения в лог отладки (--debug), который ведёт main.
    using LogFn = std::function<void(const std::string&)>;

    UpdateChecker(std::string pref_dir, std::string app_version);

    void set_logger(LogFn log);

    UpdateState state() const { return state_; }
    const std::string& version() const { return version_; }
    const std::string& tag() const { return tag_; }
    const std::string& url() const { return url_; }
    const std::string& note() const { return note_; }
    float glow() const { return glow_; }
    bool automatic() const { return automatic_; }

    // Состояние всегда пересчитывается по текущей версии: сохранённый результат
    // мог устареть (например, игру обновили или откатили).
    void set_result(UpdateState st, const std::string& ver, const std::string& url);
    void restore_cached();

    // Стартовая проверка: уходит сразу или откладывается до истечения 8 часов.
    void start_background_cycle(uint64_t now_ms);
    bool background_due(uint64_t now_ms) const;
    void schedule_next_background(uint64_t now_ms);

    void start(bool automatic);
    void poll();
    void animate_glow(uint32_t frame_delta);

    // Ручной клик не переспрашивает, если результат моложе 5 минут.
    bool result_is_fresh(uint64_t now_ms) const;
    void mark_checked();

private:
    struct Fetch {
        std::mutex mtx;
        bool done = false;
        std::string body;
    };

    void fail(const std::string& msg);
    void save_cache(uint64_t epoch, UpdateState st, const std::string& ver,
                    const std::string& url) const;
    std::string cache_path() const;

    LogFn log_;
    std::string pref_dir_;
    std::string app_version_;

    UpdateState state_ = UpdateState::Idle;
    std::string version_;
    std::string tag_;
    std::string url_;
    std::string note_;
    std::shared_ptr<Fetch> fetch_;
    Uint32 started_ms_ = 0;
    uint64_t result_ms_ = 0;     // когда получен результат (0 — кэша нет)
    uint64_t next_auto_ms_ = 0;  // когда запускать следующую фоновую проверку
    bool automatic_ = false;     // текущая проверка запущена автоматически
    float glow_ = 0.0f;          // 0..1 — подсветка пункта меню
    float glow_target_ = 0.0f;
};

}  // namespace ks
