#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ks {

std::string trim_str(const std::string& s);

// Подпись scores.json: защищает рекорд от подмены файла, но не от его удаления.
uint32_t fnv1a(const std::string& s);

// Принимает "1", "1.0", "v1.0.2" и любую строку с версией внутри ("KSnake v1.0").
bool parse_version(const std::string& s, int out[3]);

std::string format_version(const int v[3]);

// <0 — a старше b, 0 — равны, >0 — a новее b.
int compare_versions(const int a[3], const int b[3]);

// Запуск внешней программы без оболочки: args[0] — путь к программе, остальное
// — аргументы. Оболочки нет, поэтому кавычки, `;`, `&` и прочие спецсимволы в URL
// или в пути остаются данными и командой стать не могут.
//
// run_process ждёт завершения и кладёт код возврата в exit_code (nullptr — не
// нужен). spawn_detached не ждёт. Оба варианта на POSIX ставят дочернему
// процессу PR_SET_PDEATHSIG, на Windows — Job Object с KILL_ON_JOB_CLOSE, так
// что закрытие игры уносит с собой и curl, и xdg-open.
bool run_process(const std::vector<std::string>& args, int* exit_code = nullptr);
bool spawn_detached(const std::vector<std::string>& args);

#ifdef _WIN32
// Нужен ShellExecuteW в main.cpp: он принимает только wide-строку.
std::wstring utf8_to_wide(const std::string& s);
#endif

}  // namespace ks
