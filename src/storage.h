#pragma once

#include <string>

namespace ks {

// Каталог пользователя (Linux: ~/.local/share/KSnake/KSnake/); при отказе SDL —
// текущий каталог, чтобы игра оставалась работоспособной.
std::string user_pref_dir();

int load_best_score(const std::string& path);

void save_best_score(const std::string& path, int score);

}  // namespace ks
