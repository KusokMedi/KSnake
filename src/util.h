#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ks {

std::string trim_str(const std::string& s);

// Имя процесса для системных мониторов. На Linux меняет /proc/self/comm
// (prctl PR_SET_NAME); на Windows — no-op: там имя берётся из имени exe.
void set_process_name(const char* name);

// Подпись scores.json: защищает рекорд от подмены файла, но не от его удаления.
uint32_t fnv1a(const std::string& s);

// Версия — до четырёх числовых частей: "1", "1.0", "v1.0.2", "1.1.2.1"
// (четвёртая часть — номер сборки/хотфикса, как FILEVERSION в Windows).
// Дальше четвёртой части номера не бывает, лишние игнорируются. Отдельный тип,
// а не int[3], чтобы компилятор ловил вызовы со старым числом частей: иначе
// недосчитанная часть молча писалась бы за границу массива.
using Version = std::array<int, 4>;

// Принимает "1", "1.0", "v1.0.2", "1.1.2.1" и любую строку с версией внутри
// ("KSnake v1.0"). Заполняет все части (лишние — нулями).
bool parse_version(const std::string& s, Version& out);

// Печатает части до последней ненулевой, но не меньше двух ("1.1.0.0" -> "1.1",
// "1.1.0.1" -> "1.1.0.1": нулевую третью часть в середине не выбрасывает).
std::string format_version(const Version& v);

// <0 — a старше b, 0 — равны, >0 — a новее b.
int compare_versions(const Version& a, const Version& b);

// Чтение/запись файла по UTF-8 пути. SDL_GetPrefPath/SDL_GetBasePath отдают
// UTF-8, а std::ifstream/ofstream с узкой строкой на Windows трактуют байты как
// ANSI и не находят файл, если профиль пользователя не-ASCII. Поэтому на Windows
// путь переводится в UTF-16 и файл открывается через _wfopen. read_text_file
// читает всё целиком, write_text_file перезаписывает (append — дописывает).
bool read_text_file(const std::string& path, std::string& out);
bool write_text_file(const std::string& path, const std::string& body, bool append = false);
bool remove_file(const std::string& path);

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
