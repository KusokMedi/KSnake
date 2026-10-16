# AGENTS.md

Repository-level operating contract for coding agents working on **KSnake**.

This file is the authoritative source for agent conduct, engineering constraints, validation, documentation duties, and Git workflow. Source code and build files remain authoritative for the implementation that actually exists. Verify repository facts before relying on historical examples in this document.

## 1. Mission and definition of done

KSnake is a classic Snake game built with C++17, SDL2, SDL2_ttf, and CMake. The goal is a polished, complete, maintainable desktop game—not an open-ended feature experiment.

Work through the full lifecycle: inspect, clarify, plan, implement, test, fix, document, commit, and report.

A task is complete only when:
- The requested change works and existing behavior and unrelated user work are preserved.
- Relevant builds, tests, static checks, regression checks, and visual checks have been run where feasible.
- Linux and Windows 10/11 support and existing build workflows remain intact.
- Documentation accurately reflects implementation and relevant English/Russian documentation remains synchronized.
- Each completed logical work stage has a meaningful Git commit after its checks pass, if Git operations are available.
- The final report distinguishes passed, failed, not-run, blocked, and not-applicable checks.

Never claim that a command, test, build, visual check, commit, or other action succeeded unless it actually ran and the result was observed.

## 2. Authority, autonomy, and approvals

Before implementation, inspect the repository, source, build scripts, tests, documentation, and Git state. Identify ambiguity, compatibility risks, dependencies, and acceptance criteria. Ask one concise, consolidated set of questions upfront when unresolved choices could materially change the work. Avoid repeated interruptions for minor decisions that can safely follow established conventions.

The user retains authority over product direction, visible behavior, foundational technology, remote publication, and releases.

Explicit user approval is required before:
- Adding gameplay features or new user-facing capabilities.
- Materially changing UX, game rules, wording, colors, layout, timing, controls, or interaction behavior.
- Replacing or adding foundational technologies, dependencies, build systems, or core architectural principles.
- Changing the update-checking behavior defined in Section 9.
- Editing `VERSION`.
- Pushing to a remote, publishing a release, uploading release artifacts, or taking another remote publication action.

Do not infer permission from the fact that an improvement seems useful. You may identify and recommend technical improvements—such as reliability, performance, accessibility, portability, maintainability, and testability—but explain the problem, benefit, trade-offs, risks, and whether user-visible behavior changes before implementing anything that requires approval.

Deep refactoring of existing subsystems is permitted when it has a substantial, evidence-based technical benefit, stays within the current stack, and preserves observable behavior. Document significant architectural decisions and consequences.

## 3. Non-negotiable project constraints

- Keep C++17, SDL2, SDL2_ttf, CMake, and the existing dependency approach. Technology changes require a direct user request.
- Preserve Linux and Windows 10/11 support.
- Do not add gameplay features unless explicitly requested.
- Do not add dependencies without approval.
- Preserve the subsystem-oriented structure. `main.cpp` is the entry point and composition layer, not a dumping ground for unrelated logic.
- Preserve established controls, gameplay rules, screen behavior, wording, colors, layout, timing, persistence, and logs unless the task authorizes a change.
- In-game UI text is English only.
- Preserve existing run/build/release scripts and their workflow. Inspect them before changing or running them; do not recreate them blindly.
- Do not change `VERSION` unless explicitly requested.
- Never push or publish a release. The user owns those actions.
- Do not search for or read secrets, credentials, API tokens, `.env` files, passwords, SSH keys, or other private authentication material. If a task genuinely needs a secret, ask the user to supply it safely.

## 4. Repository discovery and source-of-truth rules

Before editing, inspect `git status --short --branch` and the current diff. Inspect relevant source, `CMakeLists.txt`, scripts, tests, `.gitignore`, `VERSION`, and documentation. Verify actual paths, tools, command arguments, screen names, output names, and artifact locations. Treat existing staged, unstaged, and untracked changes as user-owned unless proven otherwise.

The current source and scripts are authoritative for implementation details. This document is authoritative for agent conduct and user-approved constraints. If a documented implementation detail is stale, establish the facts and correct the relevant documentation rather than guessing. Never fabricate file contents, paths, tool availability, current Git state, or test results.

Do not run destructive cleanup or broad regeneration commands before understanding their effects. Avoid commands that can remove, overwrite, reset, or hide user work.

## 5. Architecture and module boundaries

The following is a historical module map; verify it against the current repository before relying on it:

| File | Responsibility |
|---|---|
| `src/main.cpp` | SDL initialization, input dispatch, main loop, subsystem composition |
| `src/config.h` | Board/window constants, FPS, resource/path configuration |
| `src/game.h/.cpp` | Game rules, board state, turn queue, movement, collisions; independent of SDL where possible |
| `src/ui.h/.cpp` | Fonts, UI scaling, layout, drawing, text cache |
| `src/screens.h/.cpp` | Menu, gameplay, pause, Game Over, Credits, Updates screens, hit testing |
| `src/update.h/.cpp` | Update checks, cache, background thread, GitHub API interaction, update indication |
| `src/storage.h/.cpp` | User data directory and score persistence (`scores.json`) |
| `src/version.h/.cpp` | Version reading |
| `src/util.h/.cpp` | String helpers, `parse_version`, `compare_versions`, `fnv1a` |

### Boundaries and additions
- Put a new subsystem in focused `src/<subsystem>.h` and `src/<subsystem>.cpp` files where appropriate. Avoid vague files such as `helpers.cpp`, `common.cpp`, and `misc.cpp`.
- Add new compilation units to the explicit source list in `CMakeLists.txt`.
- Put public declarations in headers and implementation details in `.cpp` files. Use anonymous namespaces for translation-unit-local details when appropriate.
- Prefer explicit state-owning types for stateful subsystems. Avoid new global mutable state and file-scope static state.
- Keep dependencies flowing in one direction and avoid include cycles. The historically documented direction is `config -> util -> storage/version -> game -> ui -> update -> screens -> main`; verify the actual graph before modifying it.
- Use `namespace ks`; do not use `using namespace`.
- Keep game, screen, and update state in their designated owning abstractions rather than duplicating it in `main.cpp`. Historically these include `Game`, `ScreenState`, and `UpdateChecker`; verify current definitions.
- Small local lambdas are fine. Logic requiring independent tests, reuse, or meaningful ownership belongs in a subsystem.
- Preserve behavior during refactoring unless the user requested a behavior change.

## 6. Code style and implementation quality

- Follow established formatting; the documented convention is four spaces.
- Use `snake_case` for variables/functions and `PascalCase` for types/enums unless the local code clearly follows another convention.
- Avoid comments that merely narrate code. Explain non-obvious reasoning (“why”) when necessary; comments should be in Russian to match existing source conventions.
- Prefer named constants and existing scaling helpers (historically `px(N)`) over unexplained magic values.
- Favor clear ownership, explicit state transitions, narrow interfaces, and deterministic behavior.
- Handle errors explicitly at subsystem boundaries. Do not silently ignore failed file operations, network results, resource loading, or thread outcomes when correctness is affected.
- Avoid speculative abstractions and unrelated cleanup. Keep changes scoped.
- Preserve compatibility with supported Linux and Windows toolchains.
- Do not suppress warnings or weaken tests just to obtain a passing result. Fix the cause or explain the limitation; obtain approval before relaxing a strict requirement.

## 7. Existing build, run, and release workflows

Verify these scripts and their actual behavior before use:

```bash
./build.sh
./start.sh
./fresh-start.sh
./build_release.sh [VERSION]
./assets/make_icons.sh
```

Historical roles:
- `build.sh`: configure/build using CMake.
- `start.sh`: build if needed and launch with diagnostic output.
- `fresh-start.sh`: remove generated build output, rebuild, and launch. Inspect side effects before running.
- `build_release.sh [VERSION]`: build local Linux AppImage and Windows standalone `.exe` artifacts; this does not authorize publication.
- `assets/make_icons.sh`: generate icon sizes and Windows `.ico` from the master SVG.

Do not replace these scripts with a new workflow without a user request. If a script is broken, investigate and repair the existing workflow where feasible.

### Warning-focused build

A historically documented clean CMake build is:

```bash
rm -rf /tmp/ksb &&
cmake -S . -B /tmp/ksb -DCMAKE_CXX_FLAGS="-Wall -Wextra" &&
cmake --build /tmp/ksb
```

Use a temporary build directory, confirm actual CMake/compiler requirements, and report the actual exit status and warnings. Do not assume that this also validates `./build.sh`.

### Release artifacts

The release script historically requires tools such as `curl`, `unzip`, `x86_64-w64-mingw32-g++`, and `x86_64-w64-mingw32-windres`; it may download `linuxdeploy` and MinGW SDL2 assets into `.release-cache/`. Verify requirements in the current script.

Historical artifact paths:
- `final_build/<VERSION>/KSnake-<VERSION>-x86_64.AppImage`
- `final_build/<VERSION>/KSnake-<VERSION>-windows-x86_64.exe`

Verify actual names and paths. Build and locally validate artifacts when relevant; never publish them. The Windows executable has historically embedded runtime/resources, including font and version information; inspect the current scripts and source before relying on old resource IDs or assumptions.

### Icons

The historical master icon is `assets/ksnake.svg`, with PNGs at 16, 24, 32, 48, 64, 128, 256, and 512 pixels plus `assets/ksnake.ico`. Verify the current set and generator.

If the icon source changes, regenerate through the existing script, validate generated content, and if feasible validate the AppImage icon and Windows PE icon resources. Do not launch the AppImage solely to inspect it if that may register it with desktop integration. Matching dimensions or a stale image viewer are not evidence that image content matches. Do not regenerate unrelated binary assets during unrelated work.

## 8. Required validation and acceptance criteria

Run all applicable checks, and report what was and was not verified.

### Baseline

Where supported by the environment:
1. Clean temporary CMake build with `-Wall -Wextra`; resolve warnings.
2. Run `./build.sh`.
3. Run available automated tests and static checks.
4. Run the headless smoke test, adapting only after checking the actual executable and arguments:

```bash
SDL_VIDEODRIVER=dummy timeout 5 ./build/snake --debug /tmp/snake.log >/dev/null
grep END /tmp/snake.log
```

The historical expected log resembles `END score=0 screen=0`. Confirm current logging semantics before treating that exact text as universal. A headless smoke test does not replace visual testing.

### Visual and UI testing

For changes to UI, screens, text, colors, layout, rendering, hit areas, or scaling, use the repository's visual harness when present.

Historical harness details to verify before use:
- `tests/make_test_source.py` copies `src/*.cpp` and `src/*.h` into `tests/build/`, injects test hooks, and may build `tests/build/snake_test`.
- `KS_SCREEN` selects screens historically including `menu`, `game`, `pause`, `gameover`, `credits`, `update`, `update_ok`, `update_ahead`, `update_check`, and `update_fail`.
- `KS_VER` overrides the version in test builds.
- `KS_W` and `KS_H` set compile-time dimensions used by the generation step.
- `KS_SHOT` and `KS_SHOTS` configure BMP screenshot output and frame numbers.
- `KS_SOFT=1` renders offscreen at exact compile-time dimensions using `SDL_CreateSoftwareRenderer`.

Verify current variable names and supported screen values. Use `KS_SOFT=1` for deterministic dimension/layout tests; a real window may have compositor-related drawable-size differences. Historically important sizes are 400×360, 960×540, and 1920×1080.

The historical checker `tests/check_shot.py` uses Pillow and optionally Tesseract to report boundary pixels, button/panel bounding boxes, colors, and recognized text. A reported `edge=0` has historically meant no light pixels at the frame boundary; verify the checker implementation and its output. If OCR is unavailable, report the limitation. Do not claim visual correctness solely because a script exits successfully. For UI refactors, inspect all affected screens/states and compare screenshots/logs where practical.

### Game logic, update logic, and storage

- Game logic: validate movement, queued turns, collision behavior, scoring, and relevant edge cases using existing or focused tests.
- Update logic: test cache expiry, failure retry, version comparison, state transitions, and visible indicators. Prefer deterministic tests over live network dependencies.
- Storage: test valid, missing, malformed, and unwritable paths where feasible, without damaging real user data.
- If source refactoring breaks deliberate test anchors, update the harness rather than silently bypassing the check.

### Platform validation

Preserve Linux and Windows 10/11 support. Build both targets when tools permit. If Windows compilation cannot run because MinGW or another dependency is missing, state that it was not verified and why; a Linux-only build is not proof of Windows compatibility.

### Temporary output

Put temporary scripts, screenshots, binaries, and logs under appropriate generated/test directories (historically `tests/build/`). Keep artifacts out of tracked paths. Before deleting temporary output, verify it was created by the current task and is not user-owned.

## 9. Update-checking behavior — frozen unless explicitly requested

Verify the implementation before editing; the following behavior must not change without a direct user request.

### States
Historical `UpdateState` values:
- `Idle`
- `Checking`
- `UpToDate`
- `Available` — a newer release exists
- `Unknown` — version is not recognized
- `Failed`
- `Ahead` — local build is newer than the latest published release

### Timing and cache
- Automatic background checking starts at application startup.
- Automatic checks occur no more often than every 8 hours (`AUTO_UPDATE_PERIOD_MS`).
- Check time/result are persisted in `update_check.txt`.
- Cached state is recalculated using `compare_versions(remote, local)`, not blindly trusted from a cached classification.
- A manual `Check for updates` does not repeat a check if the result is less than 5 minutes old (`UPDATE_CACHE_TTL_MS`).
- Failed results are not cached, permitting retries.

### UI behavior
- `Available`: the `Check for updates` button is yellow. Background checks animate the color; manual checks are immediate.
- `Ahead`: no yellow menu button and no menu marker. Explain the state only on the Updates screen, historically with `Unstable build` and `Your version is X, but latest is Y`.
- Preserve established wording and visual behavior unless the user explicitly requests changes.

### Parsing and network
- `parse_version` historically strips a leading `v` and non-digit separators and supports `X.Y.Z`; inspect actual parser/tests before changing it.
- Requests historically use `curl` on Linux/macOS or PowerShell on Windows, with a 12-second request timeout and 25-second watchdog.
- Unauthenticated GitHub API requests are subject to the public IP's rate limit. Do not add tokens or search for credentials.

Changes to the update state machine, timing, cache rules, wording, colors, network strategy, or timeout require an explicit user request.

## 10. Product behavior and persistent data

- In-game UI text is English only.
- Keep `README.md` and `README_RU.md` synchronized in meaning.
- `VERSION` is user-owned. Never edit, temporarily replace, or silently restore it unless explicitly requested.
- Preserve the current user-data directory and file formats unless asked to migrate them. Historically the path comes from `SDL_GetPrefPath("KSnake", "KSnake")`; Linux historically used `~/.local/share/KSnake/KSnake/`.
- Historical data files include `scores.json`, `update_check.txt`, and `update.json`; verify their current use in source.
- Never delete or reset real user data to make a test pass. Use temporary directories or test doubles.
- Preserve log strings when tests or user workflows may depend on them; update relevant tests/docs if a change is authorized and necessary.

## 11. Documentation responsibilities

`AGENTS.md` is the canonical agent policy, not a duplicate of every README and test procedure. Maintain coherent, purpose-specific documentation.

After changes, update documentation when public behavior, build instructions, module boundaries, configuration, data formats, dependencies, testing procedures, or workflows change. Keep `README.md` and `README_RU.md` synchronized on features, requirements, controls, build/run instructions, and user-facing behavior. Keep architecture/testing/developer docs aligned with actual source and scripts.

Prefer updating an existing document over creating a duplicate. Create specialized documentation only when it has a clear purpose and owner. Avoid repeating long procedures across files; link to a canonical explanation where practical. Never describe planned behavior as implemented. Verify paths, commands, dependencies, version examples, and claims against the repository.

## 12. Git workflow — commit logical stages; never push or publish

This section supersedes any older repository instruction prohibiting commits unless explicitly requested.

### Before changes
- Inspect `git status --short --branch` and the current diff.
- Identify pre-existing staged, unstaged, and untracked work; treat it as user-owned.
- Do not include unrelated changes in commits.
- Do not discard, overwrite, reset, or clean user work.

Never use destructive Git commands such as `git reset --hard`, `git clean`, broad checkout/restore operations, or history rewriting to make the tree look clean. Do not amend existing commits unless explicitly requested. Do not change Git configuration.

### Required commits
- Commit each completed logical stage after its applicable checks pass.
- Each commit must be a coherent, reviewable unit, not a meaningless commit made to satisfy a count.
- Inspect the staged diff before committing; stage only files belonging to the current stage.
- Use concise English commit messages, e.g. `Update screen: fix panel width`.
- If checks fail, fix and rerun them before committing, or report why the stage is blocked and ask whether the user wants to proceed with an unverified commit.
- If the environment prevents a commit, state that; never claim one exists when it does not.
- Inspect Git status after each commit and ensure no unrelated files were included.

### Generated files
Historical generated paths/patterns to exclude include `build/`, `.build-release/`, `.build-win/`, `.release-cache/`, `final_build/`, `*.AppImage`, `*.exe`, `scores.json`, and `tests/build/`. Verify `.gitignore`; add missing ignore rules only when appropriate. Never delete generated files merely to avoid committing them.

### Push and release
Never run `git push`, publish a release, upload artifacts, create a remote release, or trigger remote publication. The user personally controls push/release decisions and has an established build/release readiness workflow; preserve it. Local preparation and validation are not publication. Never change `VERSION` during release preparation unless explicitly requested.

## 13. Security and side effects

- Do not search for or read secrets, credentials, API keys, tokens, `.env` files, passwords, or private keys.
- Do not make network requests as the user unless the task requires them. The product's existing update check does not authorize unrelated network activity.
- Do not introduce analytics, telemetry, tracking, external services, or dependencies without approval.
- Inspect scripts before running those that delete directories, download binaries, alter icons, or create release artifacts.
- The AppImage may be registered by desktop integration software when launched. Avoid launching it solely for inspection if that could change desktop menus.
- Never use real user data for destructive tests.

## 14. Historical testing/tooling pitfalls — verify before relying on them

- `SDL_VIDEODRIVER=dummy` is for headless smoke tests, not visual layout validation.
- Deterministic visual testing historically requires compile-time `KS_W`/`KS_H` and `KS_SOFT=1`.
- Shell chains combining `&&` with `&` may background more of a command than intended; run generation, build, and test stages separately when timing matters.
- Visual tests may alter the update cache; use test data directories or safe backups rather than deleting real data.
- Pillow and Tesseract may be required for screenshot checks/OCR; report missing tools and impact.
- Screenshots may be large; keep them out of tracked paths.
- MinGW may be unavailable, preventing a Windows release build; report the limitation accurately.
- Image viewers may show stale images; inspect actual pixel content or extracted resources.
- Launching an AppImage may create desktop integration entries; avoid unnecessary launches.

## 15. Final report

Provide a concise factual report with:
1. **Changed:** files and behavior/architecture/documentation changes.
2. **Validation:** exact commands/checks run and observed results; for visual work include tested screens/dimensions and meaningful checker output.
3. **Commits:** hashes and messages actually created, or why no commit could be made.
4. **Not verified / blockers:** checks not run, failed checks, missing tools, and reasons.
5. **User actions:** only actions that genuinely remain for the user, such as reviewing a commit or deciding whether to push/release.
6. **Next steps:** only real outstanding work.

Never say “all tests pass” if relevant tests were not run. Explicitly distinguish passed, failed, not run, blocked by environment, and not applicable.

## 16. Definition of done

Declare the requested work complete only when all applicable conditions hold:
- Explicitly requested requirements are implemented; no unrequested gameplay features or unauthorized visible changes were introduced.
- Code meets project quality standards and respects architecture/technology constraints.
- Linux/Windows 10/11 support is preserved and actual platform validation is reported accurately.
- Relevant existing run/build/release/icon scripts remain functional.
- Relevant builds, warnings checks, tests, static analysis, smoke/regression checks, and visual checks passed, or unavailable checks are explicitly recorded without misrepresenting verification.
- Documentation matches implementation and README translations remain synchronized where relevant.
- No user-owned data/work was lost and no unrelated changes were committed.
- Each completed logical stage has a meaningful commit when Git operations are available and checks passed.
- Final diff and Git status have been inspected.
- No push or release publication occurred.

If a required condition is blocked, report the work as partially verified or blocked rather than silently lowering the standard.
