# KSnake

Classic Snake game for desktop written in C++17 with SDL2 and SDL2_ttf.

Download the game in [Releases](https://github.com/KusokMedi/KSnake/releases).

Read this in [Русский](README_RU.md).

## Features

- 20×15 grid with a resizable window
- The window can be resized, but not below 400×360: smaller sizes do not fit the layout
  (the minimum is applied on the first frame and works on every backend, Wayland included)
- Smooth snake movement: the rendering interpolates between grid steps, so the snake glides
  instead of jumping a whole cell every tick (the game rules stay grid-based)
- Rendering is VSync-synced to the monitor refresh rate (60, 120, 144, 165, 240 Hz and
  higher) with no artificial frame cap, so motion stays smooth on any display. The snake
  speed and collisions run on a fixed timer independent of the frame rate
- The snake body fades along its length from the bright head to the darker tail
- Keyboard and mouse controls (menu selection, clickable on-screen buttons)
- Pause with `Esc`, resume with `Space`, exit to menu with `Esc` again
- Game Over screen with stats (New Score, Time, Best); filling the whole board ends the
  run with `You Won!`
- Best score saved to a signed JSON file
- Update check against GitHub releases: runs in the background at startup (no more than
  once every 8 hours, the result is remembered between launches) and manually from the
  main menu
- The `Check for updates` menu item turns yellow when a new release is available
- The interface scales with the window but never larger than the base layout: fullscreen
  does not blow it up, and it only shrinks (down to a floor) on very small windows
- The background is a static full-screen picture anchored to the screen centre (a cell is
  centred on the middle of the screen); the window just shows its part, so the pattern does
  not shift when the window is resized from any corner. On Wayland sessions the game uses
  X11 (XWayland) when available, because SDL cannot report the window position on native
  Wayland. Without XWayland, or when `SDL_VIDEODRIVER` is set, the default backend is used
- The application identifies as `KSnake`: the process name (shown in system monitors) and
  the X11 `WM_CLASS`/app id are `KSnake`, and the window shows the game icon. The AppImage
  ships a matching `.desktop` (`StartupWMClass=KSnake`), and the Windows `.exe` carries the
  icon plus `VERSIONINFO`, so Task Manager shows the name and description
- Fullscreen support (F11)
- Cross-platform: Linux and Windows
- Release script producing an AppImage and a single-file Windows `.exe`
- Application icon embedded in both artifacts (`assets/ksnake.svg` is the source)

## Controls

| Action | Keys |
|---|---|
| Move | Arrow keys |
| Pause | `Esc` |
| Resume from pause | `Space` |
| Back to menu (from pause / Game Over) | `Esc` |
| Restart after Game Over | `Space` (or `R`) |
| Fullscreen | `F11` |

Pause and Game Over show clickable `Continue` / `Restart` and `Menu` buttons,
so both screens are fully usable with the mouse.
Every other on-screen hint (`Enter to exit`, `Any key to return`, ...) is clickable
as well: hover to highlight it, click to run that action.

The main menu supports both arrow keys + `Enter` and mouse. Menu items are also reachable with `1`-`6`.
`Skins` and `Settings` are placeholders and currently show `Coming soon..`.

## Requirements

- CMake 3.10 or newer
- A C++17 compiler (GCC/Clang on Linux, MinGW-w64 or MSVC on Windows)
- SDL2 and SDL2_ttf development libraries
  - Linux: `pkg-config` plus the `sdl2` and `sdl2_ttf` packages
  - Windows: SDL2/SDL2_ttf headers and libraries discoverable by CMake
- Windows release build (cross-compiled from Linux) additionally needs
  `x86_64-w64-mingw32-g++`, `x86_64-w64-mingw32-windres`, `curl` and `unzip`; the
  SDL2/SDL2_ttf MinGW development packages are downloaded into `.release-cache/`
  automatically

## Building and running

```bash
./build.sh          # configure and build into build/ (default Release)
./start.sh          # build if needed, then run build/snake with --debug to stdout
./fresh-start.sh    # remove build/, rebuild from scratch and run
./freshstart.sh     # build.sh, then start.sh
```

`build.sh` accepts a `BUILD_DIR` environment variable; the other scripts use it too.

## Creating release artifacts

```bash
./build_release.sh [VERSION]
```

The version argument defaults to the contents of [`VERSION`](VERSION). Local artifacts
are written to `final_build/<VERSION>/`:

- `KSnake-<VERSION>-x86_64.AppImage`
- `KSnake-<VERSION>-windows-x86_64.exe` — self-contained: the game, both SDL2 DLLs,
  the font and `VERSION` are embedded as Windows resources

This only builds the files locally. Publishing a release (Git tags, uploads) is a
separate, manual step and is not done by the script.

## Regenerating the icon

```bash
./assets/make_icons.sh
```

Regenerates the PNG set (16, 24, 32, 48, 64, 128, 256, 512) and `assets/ksnake.ico`
from `assets/ksnake.svg`. Requires `rsvg-convert` (librsvg) and ImageMagick (`magick`
or `convert`). The committed files in `assets/` are used as-is by the release build;
regenerate them only after editing the SVG.

## Structure

```
KSnake/
├── src/                 # game sources, one subsystem per file
│   ├── main.cpp         # composition root: SDL init, input, main loop
│   ├── config.h         # field/window constants, minimum size, font paths
│   ├── game.h/.cpp      # snake logic: field, turns, step, collisions (no SDL)
│   ├── ui.h/.cpp        # fonts, UI scale, layout, drawing primitives, text cache
│   ├── screens.h/.cpp   # screens: menu, game field, pause, game over, credits, updates
│   ├── update.h/.cpp    # update check: cache, background thread, GitHub API
│   ├── storage.h/.cpp   # pref dir, scores.json
│   ├── version.h/.cpp   # VERSION file loading
│   └── util.h/.cpp      # strings, version parsing/comparison, process helpers
├── VERSION              # current version, single source of truth
├── assets/
│   ├── font.ttf         # font
│   ├── ksnake.svg       # icon source
│   ├── ksnake-*.png     # rendered icon sizes 16-512
│   ├── ksnake.ico       # Windows icon
│   └── make_icons.sh    # regenerate PNG/ICO from ksnake.svg
├── third_party/
│   ├── nlohmann/json.hpp        # vendored JSON header
│   └── win_singlefile/singlefile.cpp  # Windows single-file exe launcher
├── tests/               # unit tests, headless runs, fuzzers, visual harness
├── docs/TESTING.md      # how to run the test harness
├── build.sh             # dev build
├── build_release.sh     # release build (AppImage + Windows exe)
├── start.sh             # run
├── fresh-start.sh       # wipe build/, rebuild from scratch and run
├── freshstart.sh        # build.sh, then start.sh
└── CMakeLists.txt       # build configuration
```

## Version

The current version lives in the [`VERSION`](VERSION) file at the repository root.
The game reads it at startup from the folder next to the binary; `build_release.sh` uses it
as the default release tag, so bumping that one file is enough to prepare a new release.

## Development and testing

[`docs/TESTING.md`](docs/TESTING.md) documents the test harness: unit tests, the
headless robustness runs, the visual matrix and the save-atomicity fuzzer. Agent conduct
and repository policy are defined in [AGENTS.md](AGENTS.md).

## License

Apache License 2.0 - see [LICENSE](LICENSE).
