// Проверка атомарности save_best_score(): держим сохранение в тесном цикле,
// а внешний скрипт убивает процесс в случайный момент. После kill файл рекорда
// обязан остаться читаемым, а его значение — согласованным.
//
// Сборка: см. tests/fuzz_save.sh
//   fuzz_save <путь> save [счётчик]   — пишет рекорд в цикле до убийства
//   fuzz_save <путь> check            — печатает прочитанный рекорд и exit code
#include <cstdio>
#include <cstdlib>

#include "../src/storage.h"

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: fuzz_save <path> save [count] | fuzz_save <path> check\n");
        return 2;
    }
    const std::string path = argv[1];
    const std::string mode = argv[2];

    if (mode == "save") {
        int count = argc > 3 ? std::atoi(argv[3]) : 20000;
        for (int i = 1; i <= count; ++i) ks::save_best_score(path, i);
        return 0;
    }
    if (mode == "check") {
        // 0 означал бы «файл не читается», поэтому на выходе всегда ждём >= 1.
        int best = ks::load_best_score(path);
        std::printf("%d\n", best);
        return best >= 1 ? 0 : 1;
    }
    std::fprintf(stderr, "unknown mode: %s\n", mode.c_str());
    return 2;
}
