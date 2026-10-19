// Windows-проверки util/storage: ветки, которые на Linux даже не компилируются.
// Требует MinGW-сборку SDL2 и Wine, поэтому запускается через
// tests/run_windows_tests.sh (см. docs/TESTING.md, раздел 9).
//
// Первый аргумент — путь к win_argdump.exe: хелпер пишет свой argv в файл,
// и по нему проверяется кавычение аргументов (quote_win_arg).
#include <direct.h>
#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "storage.h"
#include "util.h"

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

const char* kRoot = "C:\\ksnake-win-test";

std::string under(const char* rel) { return std::string(kRoot) + "\\" + rel; }

void mkdir_utf8(const std::string& path) {
    _wmkdir(utf8_to_wide(path).c_str());  // уже существует — тоже нормально
}

bool read_all(const std::string& path, std::string& out) {
    return read_text_file(path, out);
}

// Пути приходят в UTF-8, а профиль пользователя на Windows может быть
// не-ASCII: узкий std::ifstream такие пути не находит.
void test_utf8_paths() {
    const std::string dir = under("папка с пробелом");
    mkdir_utf8(dir);
    const std::string path = dir + "\\файл.txt";

    check(write_text_file(path, "привет\n"), "write to non-ascii path");
    std::string body;
    check(read_text_file(path, body) && body == "привет\n", "read back non-ascii path");
    check(remove_file(path), "remove non-ascii path");
    std::string missing;
    check(!read_text_file(path, missing), "read of removed file fails");
}

// Запись рекорда атомарна: файл-цель только заменяется, поэтому неудачная
// запись не должна затирать старый рекорд.
void test_scores() {
    const std::string path = under("scores.json");

    remove_file(path);
    check(load_best_score(path) == 0, "missing file -> 0");

    save_best_score(path, 42);
    check(load_best_score(path) == 42, "round-trip 42");

    save_best_score(path, 7);
    check(load_best_score(path) == 7, "overwrite with a lower value");

    const std::wstring wpath = utf8_to_wide(path);
    SetFileAttributesW(wpath.c_str(), FILE_ATTRIBUTE_READONLY);
    save_best_score(path, 500);
    check(load_best_score(path) == 7, "read-only target keeps the old score");
    SetFileAttributesW(wpath.c_str(), FILE_ATTRIBUTE_NORMAL);

    write_text_file(path, "not json at all");
    check(load_best_score(path) == 0, "malformed file -> 0");

    write_text_file(path, "{\"best_score\": 11, \"sign\": \"deadbeef\"}");
    check(load_best_score(path) == 0, "tampered sign -> 0");

    remove_file(path);
}

// CreateProcessW получает одну строку, поэтому аргументы кавычит quote_win_arg.
// Ошибка в кавычении меняет границы аргументов, а не только их текст: аргумент
// с пробелом и хвостовым слэшем ("C:\Program Files\") раньше съедал закрывающую
// кавычку и уезжал как "C:\Program Files"" — отсюда отдельный кейс.
void test_argv(const std::string& helper) {
    const char* cases[] = {
        "plain",
        "with space",
        "with \"quote\" inside",
        "C:\\Program Files\\",
        "trail\\",
        "back\\slash",
        "",
        "semi;colon&amp&pipe|",
        "кириллица с пробелом",
    };
    for (const char* tricky : cases) {
        const std::string out = under("args.txt");
        remove_file(out);
        int rc = -1;
        const bool ran = run_process({helper, out, tricky}, &rc);
        std::string seen;
        const bool got = ran && read_all(out, seen);
        std::string want = "[";
        want += tricky;
        want += "]\n";
        std::string label = "argv preserved: ";
        label += (*tricky == '\0') ? "(empty)" : tricky;
        if (got && seen.find(want) == std::string::npos) {
            std::printf("       helper saw: ");
            for (char c : seen) std::putchar(c == '\n' ? '|' : c);
            std::printf("\n");
        }
        check(got && seen.find(want) != std::string::npos, label);
        remove_file(out);
    }
}

void test_exit_codes() {
    int rc = -1;
    check(run_process({"C:\\windows\\system32\\cmd.exe", "/c", "exit", "3"}, &rc) && rc == 3,
          "exit code 3 propagates");
    check(!run_process({"C:\\nonexistent\\nope.exe"}, &rc),
          "missing program -> run_process false");
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: unit_win <path to win_argdump.exe>\n");
        return 2;
    }
    mkdir_utf8(kRoot);

    std::printf("unit_win: checking Windows-only paths\n");
    test_utf8_paths();
    test_scores();
    test_argv(argv[1]);
    test_exit_codes();

    _wrmdir(utf8_to_wide(under("папка с пробелом")).c_str());
    _wrmdir(utf8_to_wide(kRoot).c_str());

    std::printf("\nunit_win: pass=%d fail=%d\n", g_pass, g_fail);
    for (const std::string& s : g_failed) std::printf("  failed: %s\n", s.c_str());
    return g_fail == 0 ? 0 : 1;
}
