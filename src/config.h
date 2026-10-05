#pragma once

namespace ks {

// Поле и окно: размеры заданы здесь, чтобы их использовали и рендер, и layout.
constexpr int WIDTH = 20;
constexpr int HEIGHT = 15;
constexpr int CELL = 40;
constexpr int WIN_W = 960;
constexpr int WIN_H = 540;
constexpr int FPS = 60;

// Мелкий паттерн фона меню и экранов поверх игры.
constexpr int CHECKER_SQ = 64;

// Ниже этого размера вёрстка не проверялась, поэтому окно не сжимаем.
constexpr int MIN_WIN_W = 400;
constexpr int MIN_WIN_H = 360;

// assets/font.ttf — игровой спрайт-шрифт, в вёрстке используется как UI-шрифт;
// остальные пути нужны для Windows-сборки, где шрифт лежит в ресурсах exe.
constexpr const char* FONT_CANDIDATES[] = {
    "assets/font.ttf",
    "C:/Windows/Fonts/arial.ttf",
    "C:/Windows/Fonts/seguisb.ttf",
    "/usr/share/fonts/noto/NotoSans-Regular.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/TTF/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/Adwaita/AdwaitaSans-Regular.ttf",
};

}  // namespace ks