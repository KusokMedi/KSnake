#pragma once

#include <functional>

struct SDL_Window;

namespace ks {

// Модальный цикл Windows (перетаскивание окна мышью, ресайз за рамку) крутится
// внутри оконной процедуры, поэтому главный цикл игры в это время не выполняется
// и окно показывает последний нарисованный кадр. Пока цикл идёт, перехваченная
// оконная процедура SDL по таймеру вызывает redraw: фон и текст остаются
// свежими, а экранная привязка узора следует за окном.
// Там, где модального цикла нет (Linux, macOS), это пустышка.
void live_redraw_install(SDL_Window* window, std::function<void()> redraw);
void live_redraw_uninstall();

}  // namespace ks
