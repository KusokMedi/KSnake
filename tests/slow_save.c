/*
 * Вспомогательная библиотека для теста атомарности scores.json.
 *
 * Перехватывает open()/open64() для scores.json: после успешного открытия файл
 * уже обрезан (O_TRUNC), но данные ещё не записаны. В этот момент библиотека
 * засыпает, а тест убивает процесс сигналом SIGKILL — самый неблагоприятный
 * момент. Если запись атомарна (temp + rename), оригинал должен остаться целым.
 *
 * Нужен именно open/open64: std::ofstream в libstdc++ не идёт через fopen.
 *
 * Сборка:  g++ -shared -fPIC -O1 -Wall -Wextra -o /tmp/slowsave.so \
 *             tests/slow_save.c -ldl
 * Использование: LD_PRELOAD=/tmp/slowsave.so ./build/snake ...
 */
#include <dlfcn.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

static int (*real_open)(const char*, int, ...) = NULL;
static int (*real_open64)(const char*, int, ...) = NULL;

static void resolve(void)
{
    if (!real_open) {
        void* a = dlsym(RTLD_NEXT, "open");
        void* b = dlsym(RTLD_NEXT, "open64");
        *(void**)(&real_open) = a;
        *(void**)(&real_open64) = b;
    }
}

/* Возвращает 1, если путь — наш scores.json и файл только что обрезан. */
static int is_scores(const char* path)
{
    return path && strstr(path, "scores.json") != NULL;
}

int open(const char* path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    resolve();
    int fd = real_open(path, flags, mode);
    if (fd >= 0 && is_scores(path) && (flags & O_TRUNC)) {
        usleep(300000); /* файл пуст, пишем данные с задержкой */
    }
    return fd;
}

int open64(const char* path, int flags, ...)
{
    mode_t mode = 0;
    if (flags & O_CREAT) {
        va_list ap;
        va_start(ap, flags);
        mode = (mode_t)va_arg(ap, int);
        va_end(ap);
    }
    resolve();
    int fd = real_open64 ? real_open64(path, flags, mode) : real_open(path, flags, mode);
    if (fd >= 0 && is_scores(path) && (flags & O_TRUNC)) {
        usleep(300000);
    }
    return fd;
}
