#pragma once

#include <string>

namespace ks {

// Единственный источник версии — файл VERSION рядом с бинарником (в Windows
// он лежит в ресурсах exe). source получает "file" / "cwd" / "fallback".
std::string load_app_version(const char** source);

}  // namespace ks
