#include "version.h"

#include <SDL.h>

#include <fstream>

#include "util.h"

namespace ks {
namespace {

constexpr const char* APP_VERSION_FALLBACK = "1.0";
constexpr const char* APP_VERSION_FILE = "VERSION";

std::string read_version_file(const std::string& path) {
    std::ifstream f(path);
    std::string line;
    if (!f || !std::getline(f, line)) return std::string();
    return trim_str(line);
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
