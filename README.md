# KSnake

Classic Snake game for desktop written in C++ with SDL2 and SDL2_ttf.

Download the game in [Releases](https://github.com/KusokMedi/KSnake/releases).

Read this in [Русский](README_RU.md).

## Features

- 20×15 grid with resizeable window
- The window can be resized, but not below 400×360: smaller sizes do not fit the layout
  (the minimum is held on every backend, Wayland included)
- Keyboard and mouse controls (menu selection, clickable on-screen buttons)
- Pause with `Esc`, resume with `Space`, exit to menu with `Esc` again
- Game Over screen with stats (score, time, best score)
- Best score saved to a JSON file
- Update check against GitHub releases: runs in the background at startup (no more than
  once every 8 hours, the result is remembered between launches) and manually from the
  main menu
- The `Check for updates` menu item turns yellow when a new release is available
- Fullscreen support (F11)
- Cross-platform: Linux and Windows
- Release script producing AppImage and Windows `.exe`
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

## Structure

```
KSnake/
├── src/                 # game sources, one subsystem per file
│   ├── main.cpp         # composition root: SDL init, input, main loop
│   ├── config.h         # field/window constants and font paths
│   ├── game.h/.cpp      # snake logic: field, turns, step, collisions
│   ├── ui.h/.cpp        # fonts, UI scale, layout, drawing primitives, text cache
│   ├── screens.h/.cpp   # screens: menu, game field, pause, game over, credits, updates
│   ├── update.h/.cpp    # update check: cache, background thread, GitHub API
│   ├── storage.h/.cpp   # pref dir, scores.json
│   ├── version.h/.cpp   # VERSION file loading
│   └── util.h/.cpp      # string/version helpers
├── VERSION            # current version, single source of truth
├── assets/font.ttf    # font
├── assets/ksnake.*    # game icon, source SVG + rendered sizes + .ico
├── assets/make_icons.sh # regenerate icon PNG/ICO from assets/ksnake.svg
├── third_party/       # vendored headers (nlohmann/json) + single-file launcher
├── tests/             # unit tests, headless runs, fuzzers and the visual check
│                      # harness (not part of the build)
├── build.sh           # dev build
├── build_release.sh   # release build (AppImage + Windows exe)
├── start.sh           # run
├── fresh-start.sh     # wipe build/, rebuild from scratch and run
└── CMakeLists.txt     # build configuration
```

## Version

The current version lives in the [`VERSION`](VERSION) file at the repository root.
The game reads it at startup from the folder next to the binary; `build_release.sh` uses it
as the default release tag, so bumping that one file is enough to prepare a new release.

## License

Apache License 2.0 - see [LICENSE](LICENSE).