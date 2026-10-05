// Юнит-тесты util/version/storage KSnake.
// Требует SDL2 (storage/version используют SDL_GetPrefPath/SDL_GetBasePath).
// Сборка: ./tests/build_unit.sh
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <array>
#include <random>
#include <string>
#include <vector>

#include <SDL.h>

#define private public
#include "../src/storage.h"
#include "../src/util.h"
#include "../src/version.h"
#undef private

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

#define CHECK(...) check(__VA_ARGS__)

void write_file(const std::string& path, const std::string& body) {
    std::ofstream f(path, std::ios::binary);
    f << body;
}

void remove_file(const std::string& path) { ::remove(path.c_str()); }

std::string tmp_path(const char* name) { return std::string("/tmp/ks_unit_") + name; }

// ---------------------------------------------------------------- 4.3.1
void test_parse_version() {
    struct Case { const char* in; bool ok; int a, b, c; };
    const Case cases[] = {
        {"1.2.3", true, 1, 2, 3},
        {"v1.2.3", true, 1, 2, 3},
        {"V1.2.3", true, 1, 2, 3},
        {"1.2", true, 1, 2, 0},
        {"1", true, 1, 0, 0},
        {"", false, 0, 0, 0},
        {"abc", false, 0, 0, 0},
        {"1.2.3.4", true, 1, 2, 3},
        {"1.2.3-beta", true, 1, 2, 3},
        {"01.02.03", true, 1, 2, 3},
        {"1.10.0", true, 1, 10, 0},
        {"  1.2.3  ", true, 1, 2, 3},
        {"\n1.2.3\n", true, 1, 2, 3},
        {"\r\n1.2\r\n", true, 1, 2, 0},
        {"KSnake v1.0", true, 1, 0, 0},
        {"release-2.5.9-rc1", true, 2, 5, 9},
        {"999999999999.0.0", true, 999999, 0, 0},   // без переполнения int
        {"1.2.", true, 1, 2, 0},
        {".1.2.3", true, 1, 2, 3},
        {"0.0.0", true, 0, 0, 0},
    };
    for (const Case& c : cases) {
        int v[3] = {-7, -7, -7};
        bool ok = parse_version(c.in, v);
        CHECK(ok == c.ok, std::string("parse_version(\"") + c.in + "\") ok=" +
                             (c.ok ? "true" : "false") + " (got " + (ok ? "true" : "false") + ")");
        if (ok && c.ok) {
            CHECK(v[0] == c.a && v[1] == c.b && v[2] == c.c,
                  std::string("parse_version(\"") + c.in + "\") = " + std::to_string(c.a) + "." +
                      std::to_string(c.b) + "." + std::to_string(c.c) + " (got " +
                      std::to_string(v[0]) + "." + std::to_string(v[1]) + "." +
                      std::to_string(v[2]) + ")");
        }
    }
    // version.cpp может передать неинициализированный массив — проверяем,
    // что parse_version всё равно заполняет его целиком.
    {
        int v[3] = {5, 5, 5};
        parse_version("1.2.3", v);
        CHECK(v[0] == 1 && v[1] == 2 && v[2] == 3, "parse_version overwrites all three parts");
    }
}

void test_format_version() {
    int v[3] = {1, 2, 3};
    CHECK(format_version(v) == "1.2.3", "format_version 1.2.3");
    v[0] = 1; v[1] = 2; v[2] = 0;
    CHECK(format_version(v) == "1.2", "format_version 1.2.0 -> 1.2");
    v[0] = 0; v[1] = 0; v[2] = 0;
    CHECK(format_version(v) == "0.0", "format_version 0.0.0 -> 0.0");
    v[0] = 1; v[1] = 10; v[2] = 0;
    CHECK(format_version(v) == "1.10", "format_version 1.10.0 -> 1.10");
    // Симметрия с parse_version: то, что разобрали, должно печататься обратно
    // без потери старших разрядов.
    int rt[3];
    CHECK(parse_version("1.2.3", rt), "format round-trip: parse 1.2.3");
    CHECK(format_version(rt) == "1.2.3", "format round-trip 1.2.3 -> 1.2.3");
}

void test_compare_versions() {
    int a[3] = {1, 10, 0};
    int b[3] = {1, 9, 0};
    CHECK(compare_versions(a, b) > 0, "1.10.0 newer than 1.9.0");
    CHECK(compare_versions(b, a) < 0, "1.9.0 older than 1.10.0");
    int c[3] = {1, 9, 0};
    CHECK(compare_versions(a, c) > 0, "1.10.0 newer than 1.9.0 (copy)");
    CHECK(compare_versions(c, a) < 0, "1.9.0 (copy) older than 1.10.0");
    int d[3] = {1, 10, 0};
    CHECK(compare_versions(a, d) == 0, "equal versions compare to 0");
    CHECK(compare_versions(d, a) == 0, "equal versions compare to 0 (symmetric)");
    int e[3] = {2, 0, 0};
    CHECK(compare_versions(e, a) > 0, "2.0.0 newer than 1.10.0");
    int f[3] = {1, 10, 1};
    CHECK(compare_versions(f, a) > 0, "1.10.1 newer than 1.10.0");
    // Симметричность и транзитивность на 2000 случайных пар.
    std::mt19937 rng(12345);
    bool symmetric = true, transitive = true, antisym = true;
    std::vector<std::array<int, 3>> pool;
    for (int i = 0; i < 60; ++i)
        pool.push_back({static_cast<int>(rng() % 20), static_cast<int>(rng() % 20),
                        static_cast<int>(rng() % 5)});
    int pairs = 0;
    for (size_t i = 0; i < pool.size(); ++i) {
        for (size_t j = 0; j < pool.size(); ++j) {
            for (size_t k = 0; k < pool.size(); k += 7) {
                int x = compare_versions(pool[i].data(), pool[j].data());
                int y = compare_versions(pool[j].data(), pool[i].data());
                ++pairs;
                if ((x < 0 && !(y > 0)) || (x > 0 && !(y < 0)) || (x == 0 && y != 0))
                    symmetric = false;
                if (x > 0 && compare_versions(pool[j].data(), pool[k].data()) > 0 &&
                    !(compare_versions(pool[i].data(), pool[k].data()) > 0))
                    transitive = false;
                if (x == 0 && !(pool[i] == pool[j])) antisym = false;
            }
        }
    }
    CHECK(symmetric, "compare_versions symmetric over " + std::to_string(pairs) + " triples");
    CHECK(transitive, "compare_versions transitive");
    CHECK(antisym, "compare_versions == 0 only for equal triples");
}

// ---------------------------------------------------------------- 4.3.2
void test_fnv1a() {
    CHECK(fnv1a("") == 2166136261u, "fnv1a(\"\") = offset basis");
    CHECK(fnv1a("a") == 0xe40c292cu, "fnv1a(\"a\") = 0xe40c292c");
    CHECK(fnv1a("foobar") == 0xbf9cf968u, "fnv1a(\"foobar\") = 0xbf9cf968");
    std::string with_nul("a\0b", 3);
    CHECK(fnv1a(with_nul) != fnv1a("a"), "fnv1a distinguishes embedded NUL");
    // Стабильность на длинной строке.
    std::string big(1000000, 'x');
    uint32_t h1 = fnv1a(big);
    CHECK(h1 == fnv1a(big), "fnv1a deterministic on 1 MB string");
    // Уникальность на большом числе коротких строк (иначе подпись scores.json
    // можно подобрать).
    std::vector<uint32_t> seen;
    int collisions = 0;
    for (int i = 0; i < 100000; ++i) {
        uint32_t h = fnv1a(std::to_string(i));
        for (uint32_t o : seen)
            if (o == h) ++collisions;
        seen.push_back(h);
    }
    CHECK(collisions == 0, "fnv1a has no collisions on 100000 decimal strings");
}

void test_trim_str() {
    CHECK(trim_str("  a  ") == "a", "trim spaces");
    CHECK(trim_str("\t\r\na\r\n") == "a", "trim all whitespace");
    CHECK(trim_str("   ") == "", "trim all-whitespace -> empty");
    CHECK(trim_str("") == "", "trim empty");
    CHECK(trim_str("a b") == "a b", "trim keeps inner spaces");
}

// ---------------------------------------------------------------- 4.3.3
void test_version_file() {
    // Бинарь лежит вне репозитория, а CWD — пустой каталог: иначе
    // SDL_GetBasePath() находит чужой VERSION рядом с бинарём и source != "cwd".
    std::system("mkdir -p /tmp/ks_unit_cwd && rm -f /tmp/ks_unit_cwd/VERSION");
    if (chdir("/tmp/ks_unit_cwd") != 0) {
        CHECK(false, "chdir to a clean test directory");
        return;
    }
    unsetenv("XDG_DATA_HOME");

    struct Case { const char* body; bool present; const char* expect_source; const char* expect; };
    const Case cases[] = {
        {nullptr, false, "fallback", "1.0"},   // файла нет
        {"", true, "fallback", "1.0"},         // пустой файл
        {"\n", true, "fallback", "1.0"},       // только перевод строки
        {"1.1\n", true, "cwd", "1.1"},
        {"1.1\r\n", true, "cwd", "1.1"},
        {"  2.5.9  \n", true, "cwd", "2.5.9"},
    };
    for (const Case& c : cases) {
        remove_file("VERSION");
        if (c.present) write_file("VERSION", c.body);
        const char* src = "fallback";
        std::string v = load_app_version(&src);
        CHECK(std::string(c.expect) == v, std::string("VERSION \"") +
                                            (c.body ? c.body : "<absent>") + "\" -> \"" +
                                            c.expect + "\" (got \"" + v + "\")");
        CHECK(std::string(c.expect_source) == src,
              std::string("VERSION source for \"") + (c.body ? c.body : "<absent>") +
                  "\" is " + c.expect_source + " (got " + src + ")");
    }
    // 10 КБ мусора: берётся только первая строка.
    remove_file("VERSION");
    write_file("VERSION", std::string("3.4.5\n") + std::string(10000, 'x'));
    {
        const char* src = "fallback";
        std::string v = load_app_version(&src);
        CHECK(v == "3.4.5", "10 KB junk after the first line is ignored (got \"" + v + "\")");
    }
    // Первая строка без цифр: load_app_version отдаёт её как есть, а невалидную
    // версию отсекает main.cpp через parse_version.
    remove_file("VERSION");
    write_file("VERSION", "no digits here\n7.8.9\n");
    {
        const char* src = "fallback";
        std::string v = load_app_version(&src);
        int parsed[3] = {9, 9, 9};
        CHECK(!parse_version(v, parsed),
              "a VERSION first line without digits is rejected by parse_version (got \"" + v + "\")");
        CHECK(v == "no digits here",
              "load_app_version passes a non-version first line through verbatim; main.cpp "
              "sanitises it (got \"" + v + "\")");
    }
    remove_file("VERSION");
}

// ---------------------------------------------------------------- 4.3.4
void test_scores_json() {
    const std::string p = tmp_path("scores.json");
    struct Case { const char* body; bool present; int expect; const char* what; };
    const Case cases[] = {
        {nullptr, false, 0, "missing file -> 0"},
        {"", true, 0, "empty file -> 0"},
        {"{}", true, 0, "{} -> 0"},
        {"[]", true, 0, "[] -> 0"},
        {"null", true, 0, "null -> 0"},
        {"{broken", true, 0, "broken JSON -> 0"},
        {"{\"best_score\": 5}", true, 0, "no signature -> 0"},
        {"{\"best_score\": 5, \"sign\": 1}", true, 0, "wrong signature -> 0"},
        {"{\"best_score\": \"5\", \"sign\": 0}", true, 0, "string score -> 0"},
        {"{\"best_score\": 5.5, \"sign\": 0}", true, 0, "float score -> 0"},
        {"{\"sign\": 0}", true, 0, "signature only -> 0"},
        {"[1,2,3]", true, 0, "array -> 0"},
    };
    for (const Case& c : cases) {
        remove_file(p);
        if (c.present) write_file(p, c.body);
        int v = load_best_score(p);
        CHECK(v == c.expect, std::string(c.what) + " (got " + std::to_string(v) + ")");
    }
    // Отрицательный рекорд с корректной подписью -> 0.
    remove_file(p);
    save_best_score(p, -7);
    {
        int v = load_best_score(p);
        CHECK(v == 0, "negative best score -> 0 (got " + std::to_string(v) + ")");
    }
    // 2^63 -> не помещается в int.
    remove_file(p);
    write_file(p, "{\"best_score\": 9223372036854775807, \"sign\": 0}");
    {
        int v = load_best_score(p);
        CHECK(v == 0, "huge best score -> 0 (got " + std::to_string(v) + ")");
    }
    // Нормальный цикл save/load.
    remove_file(p);
    for (int s : {1, 7, 42, 297, 100000, 2147483647}) {
        save_best_score(p, s);
        int v = load_best_score(p);
        CHECK(v == s, "save/load round trip for " + std::to_string(s) + " (got " +
                          std::to_string(v) + ")");
    }
    // Лишнее поле ломает подпись (она считается по всему payload) — не баг,
    // но фиксируем поведение.
    remove_file(p);
    save_best_score(p, 11);
    {
        std::string body;
        { std::ifstream f(p); body.assign((std::istreambuf_iterator<char>(f)),
                                          std::istreambuf_iterator<char>()); }
        CHECK(load_best_score(p) == 11, "unknown extra field would break the signature (got " +
                                          std::to_string(load_best_score(p)) + ")");
    }
    // Каталог только для чтения, файла ещё нет: создать файл нельзя, save молча
    // игнорирует, load возвращает 0.
    {
        std::system("rm -rf /tmp/ks_unit_ro && mkdir -p /tmp/ks_unit_ro");
        std::system("chmod 500 /tmp/ks_unit_ro");
        save_best_score("/tmp/ks_unit_ro/scores.json", 99);
        CHECK(load_best_score("/tmp/ks_unit_ro/scores.json") == 0,
              "unwritable dir + no file: save silently ignored, load -> 0");
        std::system("chmod 700 /tmp/ks_unit_ro");
    }
    // Существующий файл без права записи: перезапись не удаётся, старое значение
    // сохраняется (права каталога на перезапись не влияют).
    {
        std::system("rm -rf /tmp/ks_unit_ro2 && mkdir -p /tmp/ks_unit_ro2");
        save_best_score("/tmp/ks_unit_ro2/scores.json", 3);
        std::system("chmod 400 /tmp/ks_unit_ro2/scores.json");
        save_best_score("/tmp/ks_unit_ro2/scores.json", 99);
        int got = load_best_score("/tmp/ks_unit_ro2/scores.json");
        CHECK(got == 3, "read-only scores.json: save fails silently, old value kept (got " +
                            std::to_string(got) + ")");
        std::system("chmod 700 /tmp/ks_unit_ro2/scores.json");
    }
    // 50 МБ файла: не должно быть зависания или падения.
    {
        std::string big = "{\"best_score\": 1, \"pad\": \"";
        big.append(50u * 1024u * 1024u, 'a');
        big += "\"}";
        write_file(p, big);
        int v = load_best_score(p);
        CHECK(v == 0, "50 MB scores.json -> 0 without crash (got " + std::to_string(v) + ")");
        remove_file(p);
    }
    // Путь с пробелами и кириллицей.
    {
        const std::string dir = "/tmp/ks_unit_\xd0\xbf\xd1\x80\xd0\xbe\xd0\xb1\xd0\xb5\xd0\xbb \xd1\x82\xd0\xb5\xd1\x81\xd1\x82";
        std::system(("rm -rf '" + dir + "' && mkdir -p '" + dir + "'").c_str());
        std::string f = dir + "/scores.json";
        save_best_score(f, 77);
        CHECK(load_best_score(f) == 77, "path with spaces and cyrillic works");
        std::system(("rm -rf '" + dir + "'").c_str());
    }
    // Несуществующий каталог: save молча игнорирует, load возвращает 0.
    {
        const std::string f = "/tmp/ks_unit_nodir_xyz/scores.json";
        std::system("rm -rf /tmp/ks_unit_nodir_xyz");
        save_best_score(f, 5);
        CHECK(load_best_score(f) == 0, "missing directory: save ignored, load -> 0");
    }
    // На месте файла каталог: ни save, ни load не падают.
    {
        std::system("rm -rf /tmp/ks_unit_dir && mkdir -p /tmp/ks_unit_dir");
        save_best_score("/tmp/ks_unit_dir", 5);
        CHECK(load_best_score("/tmp/ks_unit_dir") == 0, "path is a directory: no crash");
        std::system("rm -rf /tmp/ks_unit_dir");
    }
    remove_file(p);
}

// ---------------------------------------------------------------- 4.3.5
void test_atomicity_and_pref_dir() {
    // save_best_score пишет прямо в целевой файл (без tmp+rename): обрыв
    // на середине оставит невалидный JSON. Рядом tmp-файла не появляется.
    const std::string p = tmp_path("atomic.json");
    remove_file(p);
    save_best_score(p, 123);
    CHECK(load_best_score(p) == 123, "save leaves a valid file");
    CHECK(access((p + ".tmp").c_str(), F_OK) != 0,
          "no .tmp file is created next to scores.json (write is not atomic)");
    remove_file(p);
}

void test_pref_dir() {
    std::string d = user_pref_dir();
    CHECK(!d.empty(), "user_pref_dir never returns an empty string");
    CHECK(d.back() == '/', "pref dir ends with a separator (paths are concatenated directly)");
    std::printf("   user_pref_dir() = %s\n", d.c_str());
}

void test_pref_dir_subprocess() {
    // SDL кэширует pref dir, поэтому каждый XDG_DATA_HOME проверяем в отдельном
    // процессе (argv[1] != nullptr -> только этот пункт).
    struct Home { const char* env; const char* value; const char* what; };
    const Home homes[] = {
        {"XDG_DATA_HOME=/tmp/ks_xdg_plain", "/tmp/ks_xdg_plain/KSnake/KSnake/",
         "plain XDG_DATA_HOME"},
        {"XDG_DATA_HOME='/tmp/ks xdg q'", "/tmp/ks xdg q/KSnake/KSnake/",
         "XDG_DATA_HOME with a space"},
        {"XDG_DATA_HOME=\"/tmp/ks'qdg\"", "/tmp/ks'qdg/KSnake/KSnake/",
         "XDG_DATA_HOME with a single quote"},
        {"XDG_DATA_HOME=/tmp/ks-\xd1\x8e\xd0\xbd", "/tmp/ks-\xd1\x8e\xd0\xbd/KSnake/KSnake/",
         "XDG_DATA_HOME with cyrillic"},
    };
    for (const Home& h : homes) {
        // абсолютный путь: предыдущий тест уводит CWD в /tmp/ks_unit_cwd
        const char* self = getenv("KS_UNIT_SELF");
        std::string cmd =
            std::string(h.env) + " '" + (self ? self : "./ks_unit_util") + "' 1 2>/dev/null";
        FILE* pp = popen(cmd.c_str(), "r");
        std::string out;
        if (pp) {
            char buf[4096];
            while (fgets(buf, sizeof(buf), pp)) out += buf;
            pclose(pp);
        }
        while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
        std::printf("   [%s] -> %s\n", h.what, out.c_str());
        CHECK(out == h.value, std::string(h.what) + ": pref dir is \"" + h.value + "\" (got \"" +
                                out + "\")");
    }
    // Кавычка в пути к данным больше не ломает проверку обновлений: http_get()
    // зовёт run_process(), где путь уходит отдельным аргументом. Проверяем
    // настоящий путь через заглушку curl, а не копию строки.
    {
        const std::string pref_q = "/tmp/ks'qdg/KSnake/KSnake/";
        const std::string out_q = pref_q + "update.json";
        const std::string url = "https://api.github.com/x";
        // Двойные кавычки: одинарная внутри пути должна остаться символом.
        std::system("rm -rf /tmp/ks_curl_stub /tmp/ks_curl_argv.txt \"/tmp/ks'qdg\"; "
                    "mkdir -p /tmp/ks_curl_stub \"/tmp/ks'qdg/KSnake/KSnake\"");
        // Заглушка curl: пишет свои аргументы и кладёт ответ по -o.
        const std::string stub =
            "#!/bin/sh\n"
            "for a in \"$@\"; do printf '%s\\n' \"$a\" >> /tmp/ks_curl_argv.txt; done\n"
            "prev=''\n"
            "for a in \"$@\"; do\n"
            "  if [ \"$prev\" = '-o' ]; then printf '{\"tag_name\":\"v9.9.9\"}' > \"$a\"; fi\n"
            "  prev=\"$a\"\n"
            "done\n"
            "exit 0\n";
        write_file("/tmp/ks_curl_stub/curl", stub);
        std::system("chmod +x /tmp/ks_curl_stub/curl");

        std::string saved = getenv("PATH") ? getenv("PATH") : "";
        setenv("PATH", ("/tmp/ks_curl_stub:" + saved).c_str(), 1);
        int rc = -1;
        bool ran = run_process({"curl", "-fsSL", "--max-time", "12", "-H",
                                "Accept: application/vnd.github+json", "-A", "KSnake/1.1",
                                "-o", out_q, url},
                               &rc);
        setenv("PATH", saved.c_str(), 1);

        std::printf("   http_get with a quote in the pref dir -> rc=%d\n", rc);
        CHECK(ran, "http_get path: run_process starts curl");
        CHECK(rc == 0, "http_get path: curl is not broken by a quote in the pref dir (got rc=" +
                           std::to_string(rc) + ")");

        std::ifstream got(out_q, std::ios::binary);
        std::string body((std::istreambuf_iterator<char>(got)), std::istreambuf_iterator<char>());
        CHECK(body == "{\"tag_name\":\"v9.9.9\"}",
              "http_get path: the response lands in the quoted path verbatim (got \"" + body + "\")");

        std::vector<std::string> args;
        {
            std::ifstream av("/tmp/ks_curl_argv.txt");
            for (std::string line; std::getline(av, line);) args.push_back(line);
        }
        bool path_verbatim = false;
        for (const std::string& a : args)
            if (a == out_q) path_verbatim = true;
        CHECK(path_verbatim,
              "http_get path: the quoted pref dir arrives as one unsplit curl argument");
        CHECK(!args.empty() && args.front() == "-fsSL",
              "http_get path: curl receives its flags as arguments, not a shell string");
        std::system("rm -rf /tmp/ks_curl_stub /tmp/ks_curl_argv.txt \"/tmp/ks'qdg\"");
    }
    // Тот же класс проблемы в open_url (main.cpp): кавычка и `;` в URL.
    // Проверяем настоящий код — spawn_detached(), а не копию строки, иначе тест
    // проверял бы уже удалённую уязвимость и падал бы на любой версии кода.
    {
        std::system("rm -f /tmp/ks_pwned /tmp/ks_argv.txt");
        // Заглушка xdg-open: пишет каждый аргумент отдельной строкой.
        const std::string stub =
            "#!/bin/sh\n"
            "for a in \"$@\"; do printf '%s\\n' \"$a\" >> /tmp/ks_argv.txt; done\n";
        write_file("/tmp/ks_unit_xdgopen.sh", stub);
        std::system("chmod +x /tmp/ks_unit_xdgopen.sh");

        const std::string url = "https://example.com/\"; touch /tmp/ks_pwned; #";
        bool started = spawn_detached({"/tmp/ks_unit_xdgopen.sh", url});
        CHECK(started, "open_url path: spawn_detached starts the opener");
        for (int i = 0; i < 100 && access("/tmp/ks_argv.txt", F_OK) != 0; ++i) usleep(20000);

        bool pwned = access("/tmp/ks_pwned", F_OK) == 0;
        std::printf("   open_url with a quote in the URL -> injected command ran: %s\n",
                    pwned ? "YES" : "no");
        CHECK(!pwned,
              "open_url escapes quotes in the URL (an injected command must not run)");

        // URL обязан прийти в аргументе целиком: без оболочки он не режется.
        // При прямом exec скрипт не видит свой путь в "$@", поэтому строка одна.
        std::ifstream av("/tmp/ks_argv.txt");
        std::vector<std::string> args;
        for (std::string line; std::getline(av, line);) args.push_back(line);
        CHECK(args.size() == 1,
              "open_url passes exactly one argument to the opener (got " +
                  std::to_string(args.size()) + ")");
        CHECK(!args.empty() && args[0] == url,
              "open_url passes the hostile URL through verbatim, unsplit");
        std::system("rm -f /tmp/ks_pwned /tmp/ks_argv.txt /tmp/ks_unit_xdgopen.sh");
    }
}

}  // namespace

int main(int argc, char** argv) {
    (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::printf("SDL_Init failed\n");
        return 2;
    }
    if (argc > 1) {
        std::printf("%s\n", user_pref_dir().c_str());
        SDL_Quit();
        return 0;
    }
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    {
        char self[4096];
        ssize_t n = readlink("/proc/self/exe", self, sizeof(self) - 1);
        if (n > 0) {
            self[n] = 0;
            setenv("KS_UNIT_SELF", self, 1);
        }
    }
    test_parse_version();
    test_format_version();
    test_compare_versions();
    test_fnv1a();
    test_trim_str();
    test_version_file();
    test_scores_json();
    test_atomicity_and_pref_dir();
    test_pref_dir();
    test_pref_dir_subprocess();

    std::printf("\nunit_util: pass=%d fail=%d\n", g_pass, g_fail);
    for (const std::string& s : g_failed) std::printf("  failed: %s\n", s.c_str());
    return g_fail == 0 ? 0 : 1;
}
