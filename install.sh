#!/usr/bin/env bash
# install.sh — установка всего необходимого для разработки KSnake на Arch Linux.
#
# По умолчанию ставит полный набор: компилятор, CMake, SDL2/SDL2_ttf, инструменты
# релиза (кросс-компиляция Windows), Wine и утилиты для визуальных тестов,
# инструменты генерации иконок. Флаги позволяют выбрать только нужное.
#
# Использование:
#   ./install.sh                 # всё (по умолчанию)
#   ./install.sh --minimal       # только сборка игры
#   ./install.sh --with-tests    # + Wine/Python/Pillow/Tesseract
#   ./install.sh --with-release  # + MinGW-w64
#   ./install.sh --with-icons    # + librsvg/ImageMagick
#   ./install.sh --all --build   # установить всё и собрать проект
#   ./install.sh --dry-run       # показать пакеты без установки
#   ./install.sh --yes           # без подтверждений (для CI/скриптов)
set -o pipefail

# ---------- цвета ----------
if [[ -t 1 ]]; then
    C_RESET=$'\033[0m'
    C_BOLD=$'\033[1m'
    C_DIM=$'\033[2m'
    C_RED=$'\033[1;31m'
    C_GREEN=$'\033[1;32m'
    C_YELLOW=$'\033[1;33m'
    C_BLUE=$'\033[1;34m'
    C_CYAN=$'\033[1;36m'
    C_GRAY=$'\033[0;90m'
else
    C_RESET=''; C_BOLD=''; C_DIM=''; C_RED=''; C_GREEN=''; C_YELLOW=''
    C_BLUE=''; C_CYAN=''; C_GRAY=''
fi

HINT=''

say()   { printf '%s\n' "$*"; }
info()  { printf '%s[i]%s %s\n' "$C_BLUE"   "$C_RESET" "$*"; }
ok()    { printf '%s[✓]%s %s\n' "$C_GREEN"  "$C_RESET" "$*"; }
warn()  { printf '%s[!]%s %s\n' "$C_YELLOW" "$C_RESET" "$*"; }
err()   { printf '%s[✗]%s %s\n' "$C_RED"    "$C_RESET" "$*" >&2; }
step()  { echo; printf '%s==>%s %s%s%s\n' "$C_BOLD$C_CYAN" "$C_RESET" "$C_BOLD" "$*" "$C_RESET"; }
die()   {
    err "$*"
    if [[ -n "$HINT" ]]; then
        printf '    %s%s%s\n' "$C_GRAY" "$HINT" "$C_RESET" >&2
    fi
    exit 1
}

usage() {
    cat <<EOF
${C_BOLD}KSnake install.sh${C_RESET} — зависимости для разработки на Arch Linux

Использование: $0 [опции]

Режимы:
  --all            установить всё (по умолчанию)
  --minimal        только базовая сборка (компилятор, CMake, SDL2)
  --with-release   добавить кросс-компилятор MinGW-w64 и инструменты релиза
  --with-tests     добавить Wine, Python, Pillow, Tesseract для тестов
  --with-icons     добавить librsvg и ImageMagick для иконок
  --dev-tools      добавить gdb, strace, valgrind, clang, llvm, ninja

Поведение:
  --build          после установки запустить ./build.sh
  --dry-run        показать список пакетов, ничего не устанавливать
  --yes, -y        не спрашивать подтверждение (--noconfirm)
  --helper NAME    менеджер пакетов: auto (по умолчанию), pacman, yay, paru
  -h, --help       показать эту справку
EOF
}

# ---------- разбор аргументов ----------
MODE_ALL=1
WITH_RELEASE=0
WITH_TESTS=0
WITH_ICONS=0
WITH_DEV=0
BUILD=0
DRY=0
YES=0
HELPER_OVERRIDE=''

while [[ $# -gt 0 ]]; do
    case "$1" in
        --all)          MODE_ALL=1 ;;
        --minimal)      MODE_ALL=0 ;;
        --with-release) WITH_RELEASE=1 ;;
        --with-tests)   WITH_TESTS=1 ;;
        --with-icons)   WITH_ICONS=1 ;;
        --dev-tools)    WITH_DEV=1 ;;
        --build)        BUILD=1 ;;
        --dry-run)      DRY=1 ;;
        --yes|-y)       YES=1 ;;
        --helper)       shift; HELPER_OVERRIDE="${1:-}"; [[ -n "$HELPER_OVERRIDE" ]] || { err "Для --helper нужен аргумент"; exit 2; } ;;
        -h|--help)      usage; exit 0 ;;
        *)              err "Неизвестная опция: $1"; usage; exit 2 ;;
    esac
    shift
done

if [[ $MODE_ALL -eq 1 ]]; then
    WITH_RELEASE=1
    WITH_TESTS=1
    WITH_ICONS=1
fi

# ---------- баннер ----------
echo "${C_CYAN}${C_BOLD}"
cat <<'EOF'
  ╔══════════════════════════════════════════════════════════════╗
  ║   KSnake — установка зависимостей для разработки (Arch)      ║
  ╚══════════════════════════════════════════════════════════════╝
EOF
echo "${C_RESET}"
info "Режим: $([[ $MODE_ALL -eq 1 ]] && echo 'полный (--all)' || echo 'выборочный')"
info "Компоненты: базовые$([[ $WITH_RELEASE -eq 1 ]] && echo ' + релиз')$([[ $WITH_TESTS -eq 1 ]] && echo ' + тесты')$([[ $WITH_ICONS -eq 1 ]] && echo ' + иконки')$([[ $WITH_DEV -eq 1 ]] && echo ' + dev-tools')"

# ---------- проверка окружения ----------
step "Проверка окружения"

if [[ ! -r /etc/os-release ]]; then
    die "Не найден /etc/os-release — не похоже на Linux."
fi
# shellcheck disable=SC1091
. /etc/os-release
if [[ "${ID:-}" != "arch" && "${ID_LIKE:-}" != *arch* ]]; then
    err "Это не Arch-based система (ID=${ID:-?}, ID_LIKE=${ID_LIKE:-?})."
    HINT="Скрипт предназначен для Arch Linux и его производных (Omarchy, Manjaro, EndeavourOS)."
    die "Продолжение невозможно."
fi
ok "ОС: ${PRETTY_NAME:-$ID} (${ID:-?})"

if ! command -v pacman >/dev/null 2>&1; then
    die "pacman не найден — установка пакетов невозможна."
fi
ok "pacman: $(pacman --version | sed -n 's/.*\(Pacman v[^ ]*\).*/\1/p' | head -1)"

# sudo
if [[ $EUID -eq 0 ]]; then
    info "Запущено от root — sudo не требуется."
    SUDO=()
else
    if ! command -v sudo >/dev/null 2>&1; then
        die "sudo не найден. Запустите скрипт от root или установите sudo."
    fi
    if ! sudo -n true 2>/dev/null; then
        if [[ -t 0 ]]; then
            info "Нужны права root. Введите пароль sudo:"
            sudo -v || die "Не удалось получить права sudo."
        else
            HINT="Запустите с --yes от root или настройте NOPASSWD для pacman."
            die "sudo требует пароль, но терминал не интерактивный."
        fi
    fi
    SUDO=(sudo)
    ok "sudo доступен (неинтерактивно)."
fi

# менеджер пакетов
if [[ -n "$HELPER_OVERRIDE" ]]; then
    HELPER="$HELPER_OVERRIDE"
else
    if command -v yay >/dev/null 2>&1; then
        HELPER="yay"
    elif command -v paru >/dev/null 2>&1; then
        HELPER="paru"
    else
        HELPER="pacman"
    fi
fi
if [[ $EUID -eq 0 && "$HELPER" != "pacman" ]]; then
    warn "Запуск от root: $HELPER не работает от root, используем pacman."
    HELPER="pacman"
fi
if ! command -v "$HELPER" >/dev/null 2>&1; then
    die "Менеджер пакетов не найден: $HELPER"
fi
ok "Менеджер пакетов: $HELPER"

# блокировка / параллельные транзакции
if [[ -f /var/lib/pacman/db.lck ]]; then
    HINT="Удалите блокировку только если pacman действительно не работает: sudo rm /var/lib/pacman/db.lck"
    die "Обнаружена блокировка pacman (/var/lib/pacman/db.lck)."
fi
if pgrep -x pacman >/dev/null 2>&1 || pgrep -x yay >/dev/null 2>&1 || pgrep -x paru >/dev/null 2>&1; then
    die "Обнаружен запущенный pacman/yay/paru. Дождитесь завершения."
fi
ok "Нет активных транзакций пакетов."

# интернет
step "Проверка сети"
if ! curl -fsS --max-time 8 -o /dev/null https://archlinux.org 2>/dev/null; then
    HINT="Проверьте подключение, DNS и прокси. Нужен доступ к зеркалам Arch."
    die "Нет доступа к интернету (archlinux.org недоступен)."
fi
ok "Интернет доступен."

# ---------- выбор пакетов ----------
step "Формирование списка пакетов"

# SDL2: в новых Arch пакет называется sdl2-compat, в старых — sdl2.
if pacman -Si sdl2-compat >/dev/null 2>&1; then
    SDL_PKG="sdl2-compat"
elif pacman -Si sdl2 >/dev/null 2>&1; then
    SDL_PKG="sdl2"
else
    HINT="Установите SDL2 вручную или проверьте репозитории (extra)."
    die "Не найден пакет SDL2 (sdl2/sdl2-compat)."
fi

CORE_PKGS=(base-devel cmake pkgconf git curl unzip "$SDL_PKG" sdl2_ttf)
RELEASE_PKGS=(mingw-w64-gcc mingw-w64-binutils)
TEST_PKGS=(wine python python-pillow tesseract)
ICON_PKGS=(librsvg imagemagick)
DEV_PKGS=(gdb strace valgrind clang llvm ninja)

PACKAGES=("${CORE_PKGS[@]}")
[[ $WITH_RELEASE -eq 1 ]] && PACKAGES+=("${RELEASE_PKGS[@]}")
[[ $WITH_TESTS   -eq 1 ]] && PACKAGES+=("${TEST_PKGS[@]}")
[[ $WITH_ICONS   -eq 1 ]] && PACKAGES+=("${ICON_PKGS[@]}")
[[ $WITH_DEV     -eq 1 ]] && PACKAGES+=("${DEV_PKGS[@]}")

# дедупликация с сохранением порядка
mapfile -t PACKAGES < <(printf '%s\n' "${PACKAGES[@]}" | awk '!seen[$0]++')

info "SDL2-пакет: $SDL_PKG"
info "Всего пакетов: ${#PACKAGES[@]}"
printf '%s\n' "${PACKAGES[@]}" | sed 's/^/    - /'

# ---------- проверка наличия в репозиториях ----------
step "Проверка пакетов в репозиториях"
NOT_FOUND=()
for pkg in "${PACKAGES[@]}"; do
    if ! pacman -Si "$pkg" >/dev/null 2>&1; then
        NOT_FOUND+=("$pkg")
    fi
done
if (( ${#NOT_FOUND[@]} > 0 )); then
    err "Пакеты не найдены в репозиториях: ${NOT_FOUND[*]}"
    HINT="Проверьте названия и подключённые репозитории (core/extra/multilib)."
    die "Прервано до установки."
fi
ok "Все ${#PACKAGES[@]} пакетов найдены."

# ---------- обновление базы ----------
step "Синхронизация базы пакетов"
if [[ $DRY -eq 1 ]]; then
    info "dry-run: пропускаем синхронизацию."
else
    if [[ "$HELPER" == "pacman" ]]; then
        if [[ $EUID -eq 0 ]]; then
            pacman -Sy ${YES:+--noconfirm}
        else
            sudo pacman -Sy ${YES:+--noconfirm}
        fi
    else
        "$HELPER" -Sy ${YES:+--noconfirm}
    fi
    rc=$?
    if [[ $rc -ne 0 ]]; then
        HINT="Проверьте зеркала: sudo pacman -Syy или отредактируйте /etc/pacman.d/mirrorlist."
        die "Не удалось синхронизировать базу пакетов (код $rc)."
    fi
    ok "База пакетов синхронизирована."
fi

# ---------- установка ----------
step "Установка пакетов"
if [[ $DRY -eq 1 ]]; then
    info "dry-run: установка пропущена. Команда была бы:"
    if [[ "$HELPER" == "pacman" ]]; then
        printf '    sudo pacman -S --needed %s %s\n' "${YES:+--noconfirm}" "${PACKAGES[*]}"
    else
        printf '    %s -S --needed %s %s\n' "$HELPER" "${YES:+--noconfirm}" "${PACKAGES[*]}"
    fi
else
    if [[ "$HELPER" == "pacman" ]]; then
        if [[ $EUID -eq 0 ]]; then
            pacman -S --needed ${YES:+--noconfirm} "${PACKAGES[@]}"
        else
            sudo pacman -S --needed ${YES:+--noconfirm} "${PACKAGES[@]}"
        fi
    else
        "$HELPER" -S --needed ${YES:+--noconfirm} "${PACKAGES[@]}"
    fi
    rc=$?
    if [[ $rc -ne 0 ]]; then
        err "Установка завершилась с кодом $rc."
        HINT="Смотрите вывод выше. Частые причины: устаревшая база, блокировка db.lck, конфликт файлов, зеркала."
        die "Не удалось установить зависимости."
    fi
    ok "Пакеты установлены (или уже были установлены)."
fi

# ---------- проверка результата ----------
step "Проверка установленных инструментов"

check() {
    local cmd="$1"; shift
    if command -v "$cmd" >/dev/null 2>&1; then
        local ver
        ver="$("$cmd" "$@" 2>/dev/null | head -1)"
        [[ -n "$ver" ]] || ver="найден"
        ok "$cmd — $ver"
    else
        warn "$cmd — НЕ НАЙДЕН"
    fi
}

check cmake --version
check g++ --version
check pkg-config --version
SDL_TTF_MOD=""
for m in SDL2_ttf sdl2_ttf; do
    if pkg-config --exists "$m" 2>/dev/null; then SDL_TTF_MOD="$m"; break; fi
done
if pkg-config --exists sdl2 2>/dev/null && [[ -n "$SDL_TTF_MOD" ]]; then
    ok "SDL2 $(pkg-config --modversion sdl2), SDL2_ttf $(pkg-config --modversion "$SDL_TTF_MOD") (pkg-config: $SDL_TTF_MOD)"
else
    # CMake ищет SDL2_ttf через find_path/find_library, а не pkg-config.
    if [[ -f /usr/include/SDL2/SDL_ttf.h || -f /usr/local/include/SDL2/SDL_ttf.h ]]; then
        ok "SDL2/SDL2_ttf найдены (заголовки на месте; pkg-config-модуль: ${SDL_TTF_MOD:-нет})"
    else
        warn "SDL2/SDL2_ttf не найдены (нет заголовков)"
    fi
fi
check git --version
if command -v curl >/dev/null 2>&1; then ok "curl — $(curl --version | head -1 | awk '{print $1, $2}')"; else warn "curl — НЕ НАЙДЕН"; fi

if [[ $WITH_RELEASE -eq 1 ]]; then
    check x86_64-w64-mingw32-g++ --version
    check x86_64-w64-mingw32-windres --version
    check unzip -v
fi
if [[ $WITH_TESTS -eq 1 ]]; then
    check wine --version
    check python3 --version
    check tesseract --version
    if python3 -c 'import PIL; print(PIL.__version__)' >/dev/null 2>&1; then
        ok "Pillow — $(python3 -c 'import PIL; print(PIL.__version__)')"
    else
        warn "Pillow (python) — НЕ НАЙДЕН"
    fi
fi
if [[ $WITH_ICONS -eq 1 ]]; then
    check rsvg-convert --version
    if command -v magick >/dev/null 2>&1; then
        check magick --version
    elif command -v convert >/dev/null 2>&1; then
        check convert --version
    else
        warn "ImageMagick (magick/convert) — НЕ НАЙДЕН"
    fi
fi
if [[ $WITH_DEV -eq 1 ]]; then
    check gdb --version
    check clang --version
    check ninja --version
fi

# ---------- сборка (опционально) ----------
if [[ $BUILD -eq 1 ]]; then
    step "Сборка KSnake"
    if [[ -x ./build.sh ]]; then
        if ./build.sh; then
            ok "Сборка успешна: ./build/snake"
        else
            err "Сборка завершилась с ошибкой."
            exit 1
        fi
    else
        warn "build.sh не найден или не исполняем — пропускаем."
    fi
fi

# ---------- итог ----------
step "Готово"
ok "Зависимости для разработки KSnake установлены."
echo
info "Следующие шаги:"
echo "    ${C_BOLD}./build.sh${C_RESET}            # собрать игру"
echo "    ${C_BOLD}./start.sh${C_RESET}            # собрать и запустить"
echo "    ${C_BOLD}./freshstart.sh${C_RESET}        # полная пересборка и запуск"
[[ $WITH_RELEASE -eq 1 ]] && echo "    ${C_BOLD}./build_release.sh${C_RESET}   # собрать релиз (AppImage + Windows exe)"
[[ $WITH_TESTS   -eq 1 ]] && echo "    ${C_BOLD}./tests/run_headless.sh${C_RESET}  # headless-тесты"
[[ $WITH_ICONS   -eq 1 ]] && echo "    ${C_BOLD}./assets/make_icons.sh${C_RESET}   # пересобрать иконки"
echo
info "Headless smoke-тест:"
echo "    ${C_DIM}SDL_VIDEODRIVER=dummy timeout 5 ./build/snake --debug /tmp/snake.log; grep END /tmp/snake.log${C_RESET}"
echo
ok "Удачной разработки!"
