# KSnake

Classic Snake game for desktop written in C++ with SDL2 and SDL2_ttf.

Download the game in [Releases](https://github.com/KusokMedi/KSnake/releases).

## Features

- 20×15 grid with resizeable window
- Keyboard and mouse controls (menu selection, clickable links)
- Pause (automatic on focus loss / minimize)
- Game Over screen with stats (score, time, best score)
- Best score saved to a JSON file
- Fullscreen support (F11)
- Cross-platform: Linux and Windows
- Release script producing AppImage and Windows `.zip`

## Controls

| Action | Keys |
|---|---|
| Move | Arrow keys |
| Turn right/left | Arrow keys |
| Pause | `P` or `Space` |
| Menu | `Esc` |
| Fullscreen | `F11` |
| Restart after Game Over | `R` |

The main menu supports both arrow keys + `Enter` and mouse.

## Structure

```
KSnake/
├── src/main.cpp       # game (logic and rendering)
├── assets/font.ttf    # font
├── third_party/       # vendored headers (nlohmann/json)
├── build.sh           # dev build
├── build_release.sh   # release build (AppImage + Windows zip)
├── start.sh           # run
└── CMakeLists.txt     # build configuration
```

## License

MIT - see [LICENSE](LICENSE).