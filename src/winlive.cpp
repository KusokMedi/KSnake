#include "winlive.h"

#if defined(_WIN32)

#include <SDL.h>
#include <SDL_syswm.h>
#include <windows.h>

#include <utility>

namespace ks {
namespace {

// Тик модального цикла — примерно кадр на 60 Гц. Рисуем по таймеру, а не на
// каждое WM_MOVE/WM_SIZE: Present внутри обработчика останавливает протяжку
// окна на время кадра, и делать это на каждое движение мыши незачем.
constexpr UINT kRedrawTimerId = 0x4B53;  // 'KS'
constexpr UINT kRedrawIntervalMs = 16;

std::function<void()> g_redraw;
HWND g_hwnd = nullptr;
WNDPROC g_prev_proc = nullptr;
bool g_redrawing = false;

LRESULT CALLBACK live_window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_ENTERSIZEMOVE:
        case WM_ENTERMENULOOP:
            SetTimer(hwnd, kRedrawTimerId, kRedrawIntervalMs, nullptr);
            break;
        case WM_EXITSIZEMOVE:
        case WM_EXITMENULOOP:
            KillTimer(hwnd, kRedrawTimerId);
            break;
        case WM_TIMER:
            if (wparam == kRedrawTimerId) {
                // Повторный вход возможен, если SDL внутри Present сама прокачает
                // очередь сообщений: второй кадр поверх первого не нужен.
                if (!g_redrawing && g_redraw) {
                    g_redrawing = true;
                    g_redraw();
                    g_redrawing = false;
                }
                return 0;
            }
            break;
        case WM_NCDESTROY: {
            KillTimer(hwnd, kRedrawTimerId);
            WNDPROC prev = g_prev_proc;
            if (hwnd == g_hwnd) {
                if (prev) {
                    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(prev));
                }
                g_hwnd = nullptr;
                g_prev_proc = nullptr;
            }
            if (prev) return CallWindowProc(prev, hwnd, msg, wparam, lparam);
            return DefWindowProc(hwnd, msg, wparam, lparam);
        }
        default:
            break;
    }
    if (g_prev_proc) return CallWindowProc(g_prev_proc, hwnd, msg, wparam, lparam);
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

}  // namespace

void live_redraw_install(SDL_Window* window, std::function<void()> redraw)
{
    g_redraw = std::move(redraw);
    if (!window) return;

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE) return;
    if (info.subsystem != SDL_SYSWM_WINDOWS) return;
    HWND hwnd = info.info.win.window;
    if (!hwnd || hwnd == g_hwnd) return;

    // Окно пересоздают при смене режима фуллскрина: подписываемся заново на
    // новый HWND, прежний к этому моменту уже получил WM_NCDESTROY.
    g_hwnd = hwnd;
    g_prev_proc = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(live_window_proc)));
    if (!g_prev_proc) g_hwnd = nullptr;
}

void live_redraw_uninstall()
{
    if (g_hwnd) {
        KillTimer(g_hwnd, kRedrawTimerId);
        if (g_prev_proc) {
            SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_prev_proc));
        }
    }
    g_hwnd = nullptr;
    g_prev_proc = nullptr;
    g_redraw = nullptr;
}

}  // namespace ks

#else  // !_WIN32

namespace ks {

void live_redraw_install(SDL_Window*, std::function<void()>) {}
void live_redraw_uninstall() {}

}  // namespace ks

#endif
