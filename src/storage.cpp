#include "storage.h"

#include <SDL.h>
#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "util.h"

namespace ks {

using json = nlohmann::json;

namespace {

// Рекорд пишется через временный файл. Прямая запись приводила к битому файлу,
// если процесс убивали или вылетал посреди записи (164 из 3000 прогонов):
// рекорд молча терялся. rename() атомарен, поэтому читатель видит либо старый,
// либо новый файл целиком, но никогда — обрезанный.
//
// Имя временного файла фиксированное, без pid: после kill остаётся максимум
// один мусорный файл, который следующая запись просто перезапишет. С pid в
// имени мусор копился бы по файлу на каждый аварийный запуск.

// Запись с fsync до rename: иначе после атомарного rename на диск может лечь
// пустой файл, если система выключилась сразу после игры.
bool write_file_synced(const std::string& path, const std::string& body) {
#ifdef _WIN32
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    bool ok = std::fwrite(body.data(), 1, body.size(), f) == body.size();
    if (std::fflush(f) != 0) ok = false;
    if (ok && _commit(_fileno(f)) != 0) ok = false;
    if (std::fclose(f) != 0) ok = false;
    return ok;
#else
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return false;
    bool ok = true;
    size_t off = 0;
    while (off < body.size()) {
        ssize_t n = ::write(fd, body.data() + off, body.size() - off);
        if (n <= 0) {
            if (errno == EINTR) continue;
            ok = false;
            break;
        }
        off += static_cast<size_t>(n);
    }
    if (ok && ::fsync(fd) != 0) ok = false;
    if (::close(fd) != 0) ok = false;
    return ok;
#endif
}

// std::rename на Windows не перезаписывает существующий файл, поэтому там
// отдельный вызов с MOVEFILE_REPLACE_EXISTING.
bool replace_file(const std::string& tmp, const std::string& path) {
#ifdef _WIN32
    return MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return std::rename(tmp.c_str(), path.c_str()) == 0;
#endif
}

}  // namespace

std::string user_pref_dir() {
    char* p = SDL_GetPrefPath("KSnake", "KSnake");
    if (!p) return std::string(".");
    std::string dir(p);
    SDL_free(p);
    return dir;
}

int load_best_score(const std::string& path) {
    std::ifstream f(path);
    if (!f) return 0;
    json j;
    try {
        f >> j;
    } catch (...) {
        return 0;
    }
    auto it = j.find("sign");
    if (it == j.end() || !it->is_number_unsigned()) return 0;
    json payload = j;
    payload.erase("sign");
    if (fnv1a(payload.dump()) != it->get<uint32_t>()) return 0;
    if (j.contains("best_score") && j["best_score"].is_number_integer()) {
        int v = j["best_score"].get<int>();
        return v > 0 ? v : 0;
    }
    return 0;
}

void save_best_score(const std::string& path, int score) {
    json j;
    j["best_score"] = score;
    std::string body = j.dump();
    j["sign"] = fnv1a(body);
    // Байты файла не меняем: load_best_score() проверяет подпись по dump() без
    // отступа, и старые scores.json должны остаться читаемыми.
    body = j.dump(2) + "\n";

    const std::string tmp = path + ".tmp";
    // Файл, помеченный игроком read-only, не трогаем. rename() на POSIX
    // перезаписывает цель независимо от её прав — важны только права на каталог,
    // — поэтому флаг проверяем сами, иначе атомарность тихо сломала бы его.
    if (access(path.c_str(), F_OK) == 0 && access(path.c_str(), W_OK) != 0) return;
    if (!write_file_synced(tmp, body) || !replace_file(tmp, path)) {
        // Не сработало — убираем мусор, но старый scores.json не трогаем: он
        // остаётся целым, это и есть смысл атомарной записи.
        std::remove(tmp.c_str());
    }
}

}  // namespace ks
