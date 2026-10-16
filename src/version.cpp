#include "version.h"

#include <SDL.h>

#include <string>

#include "util.h"

namespace ks {
namespace {

constexpr const char* APP_VERSION_FALLBACK = "1.0";
constexpr const char* APP_VERSION_FILE = "VERSION";

std::string read_version_file(const std::string& path) {
    std::string body;
    if (!read_text_file(path, body)) return std::string();
    size_t end = body.find_first_of("\r\n");
    return trim_str(body.substr(0, end));
}

}  // namespace

std::string load_app_version(const char** source) {
    char* base = SDL_GetBasePath();
    if (base) {
        std::string v = read_version_file(std::string(base) + APP_VERSION_FILE);
        SDL_free(base);
        if (!v.empty()) {
            *source = "file";
            return v;
        }
    }
    std::string v = read_version_file(APP_VERSION_FILE);
    if (!v.empty()) {
        *source = "cwd";
        return v;
    }
    *source = "fallback";
    return APP_VERSION_FALLBACK;
}

}  // namespace ks
