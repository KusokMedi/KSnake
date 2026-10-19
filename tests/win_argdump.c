/* Хелпер для tests/unit_win.cpp: пишет свой argv в файл (argv[1]), чтобы тест
 * мог проверить, как KSnake передаёт аргументы через CreateProcessW.
 * Программа wide (wmain, -municode): узкий main получил бы argv уже
 * перекодированным в ANSI-кодовую страницу, и кириллица в аргументе
 * превратилась бы в "?" — проверять было бы нечего.
 * Собирается tests/run_windows_tests.sh, на Linux не нужен. */
#include <stdio.h>
#include <windows.h>

int wmain(int argc, wchar_t** argv) {
    if (argc < 2) return 2;
    FILE* f = _wfopen(argv[1], L"wb");
    if (!f) return 3;
    fprintf(f, "argc=%d\n", argc);
    for (int i = 0; i < argc; ++i) {
        char utf8[4096];
        int n = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, utf8, sizeof(utf8), NULL, NULL);
        fprintf(f, "[%s]\n", n > 0 ? utf8 : "?");
    }
    fclose(f);
    return 0;
}
