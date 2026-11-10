# Development and diagnostics

## Build and local testing

### Isolated Windows testing

Double-click **test-local.cmd** to build, run tests, and open a local preview.
Sign-in tests are excluded from this workflow.
Edit source files here, close the preview, and run it again to test a change.
The installed app can keep running. This workflow never installs or publishes.

The preview uses `build/local-test/`, with separate preferences and learned words
in `build/local-test/local-profile/` and a separate copy of the keyboard pack.
It starts with defaults and keeps its own settings between runs. Windows startup,
sign-in changes, desktop keyboard/mouse output, and eye tracking are disabled in
this build. Use it for appearance, layouts, and settings; hardware and desktop
input need separate acceptance testing.

In PowerShell, use `.\test-local.ps1`. Use `-NoBuild` to reopen the last build,
or `-SkipTests` for a faster rebuild after tests have passed. To start with fresh
test preferences, close the preview and rename `build/local-test/local-profile`.
Your installed app's settings are never copied or modified. Keep experimental
keyboard files inside the test build; opening an external XML file in the editor
and saving it still modifies that selected file.

```sh
cd /path/to/GazeGrid
./build.sh
./run.sh
```

CMake and Ninja are installed locally in `.tools/`. The build uses the host Qt 6.12 or newer development packages, layer-shell-qt, libei and libspeechd (Speech Dispatcher). Settings also uses the KDE Kirigami and QQC2 desktop-style QML modules, with Qt Widgets providing native desktop control styling. No system helper was installed. `build.sh` defaults to an optimized `RelWithDebInfo` build with debugging symbols and runs the test suite. Use `./build.sh -DCMAKE_BUILD_TYPE=Debug` for an unoptimized debugging build. Runtime settings and caches stay under `runtime/` when using `run.sh`.

The default SDK is the exact user-supplied `stream_engine_linux_3.0.4.6031` directory. Its headers are used for compilation and its x64 shared library is loaded by absolute path at runtime. The sibling 4.24 SDK is not used. SDK files are not copied into this project. The CMake cache variable `TOBII_SDK` allows relocating the same SDK.

The Tobii worker polls subscribed callbacks approximately every 8 ms and manually synchronizes the tracker clock every 30 seconds. Synchronization is also performed on connection and reconnection; temporary synchronization failures retry after one second. All tracker processing stays on the worker thread.

## Diagnostics and desktop checks

```sh
./run.sh --diagnose --validate-keyboards
./run.sh --smoke
./run.sh --portal-check
./run.sh --input-check
./run.sh --pointer-check
./run.sh --magnifier-check
python3 tests/render_layouts.py
```

`--diagnose` loads the real library and enumerates devices, and queries portal capabilities. It prints no device URLs or serial numbers. A zero device count is not live-gaze success.

`--gaze` starts with gaze enabled. `--gaze-check` runs a 20-second live check through
the Tobii worker, controller and QML interface, with dwell disabled and output
paused. It reports aggregate valid-sample and keyboard-hit counts, without
recording individual gaze coordinates or requesting desktop authorization.

`--validate-keyboards` verifies XML parsing, geometry, command syntax and local navigation targets. It reports per-layout actions, unsupported actions, symbols and attribute names, preserving command order and Text whitespace. Implementation categories are not an action compatibility certification; `behavioralParityVerified` remains false.

`--smoke` displays the panel briefly and closes it. Set `GAZE_GRID_SCREENSHOT` to a file path to capture the panel itself.

`--portal-check` opens a permission session and waits for usable libei devices without emitting input. `--input-check` opens a separate disposable receiver after authorization, waits for receiver focus, and sends `gaze-grid` plus Enter. It reports receipt and closes. Click the receiver if KDE does not activate it automatically. Do not switch focus during this short test. The check also clicks the XML q key and verifies that the receiver keeps focus and receives q. It stops if the receiver loses focus. Passing does not establish browser/XWayland compatibility.

See [historical acceptance](archive/host-acceptance.md) and the [roadmap](roadmap.md) for evidence and outstanding gates.

`tests/render_layouts.py` creates offscreen snapshots and a coverage report in `runtime/render-audit/`, using an isolated temporary profile. The build test suite includes a fake screenshot portal on an isolated D-Bus session (`dbus-run-session`); it never invokes the real screenshot portal. Restricted sandboxes must allow creation of the local test-bus socket.

`--pointer-check` opens a disposable full-screen Wayland receiver and exercises targeted left click, double-click, drag, scroll and right click. It targets the primary display and stops on receiver focus loss. Keep that receiver focused during the short test. `--settings` opens settings without changing the input pause state.

`--magnifier-check` verifies screenshot capture and a precise target click against the disposable receiver. Keep it focused and approve capture if prompted.

Settings can opt into remembering desktop-control permission where the portal supports it. Save preferences to retain the choice and automatically connect keyboard/mouse control on subsequent launches. The desktop may still ask for permission if it cannot reuse the saved authorization. Diagnostic and smoke modes do not automatically connect; input checks manage their own sessions. Restore tokens are stored with owner-only permissions and consumed once before requesting a replacement. **Disconnect and forget desktop permission** closes input and deletes the local token; save preferences afterward to keep automatic connection disabled on future launches. Desktop permission management remains available in KDE settings.

## Experimental Plasma input method

`--input-method` must be launched by KWin with its inherited Wayland socket.
CMake generates `build/org.gaze_grid.gaze_grid.InputMethod.desktop` pointing to this build;
the build does not automatically install or select it. The renamed entry must be
registered before it can be selected in Plasma’s virtual keyboard settings.

Run `python3 tests/native_input_method.py` to check 22 top/bottom placement-button
clicks, focus retention, Unicode commits, Backspace, Return and password-field
input against a separate Qt receiver in a private,
headless KWin session. The test needs permission to create local D-Bus sockets.
It writes `runtime/native-input-method.log` and does not change the desktop's
keyboard selection. The receiver keeps its original field focused. Add `--late-start` to delay the
keyboard by one second and verify recovery of that already focused field.

Native mode supports top and bottom layouts. The first placement key moves the
keyboard between them. Top placement uses a non-focus-taking overlay; returning
to the bottom restores the native input panel. Native text input stays connected
through both transitions. Settings is a regular movable, resizable desktop window. Top mode
covers application content instead of asking applications to resize, and remains
visible outside text fields for gaze/pointer controls. Plasma's native panel
visibility controls apply to bottom placement. Offset/minimized layouts still
require standalone mode and are labeled unavailable in Plasma mode.
Pointer actions and held physical shortcuts still require desktop authorization.
Real-session focus/recovery, GTK/browser/XWayland compatibility, and gaze
acceptance remain outstanding. Selectable registration alone does not establish
that gaze-grid is ready to replace Plasma Keyboard.

## Layout and local composition tests

For isolated Windows desktop acceptance, build the optional `windows-layout-acceptance` target and run it in an unlocked VM with `GAZE_GRID_TEST_WINDOWS_LAYOUTS=1`. It is excluded from the default build and CTest. It uses a separate target editor to compare labels with real physical typing under US, French, German and Russian layouts, checks Shift/Caps Lock/AltGr, pending dead keys and settings focus, then restores the original layout and Caps Lock state. Key-down events are rejected if another application takes foreground focus. Run this acceptance check when changing platform input, layout tracking or focus behavior; ordinary development uses the normal automated suites.

`GAZE_GRID_TEST_LOCAL_IME=1` enables optional real-engine tests. They verify English key routing, Pinyin candidate commitment of `你好`, and Japanese Roman input producing `日本`, editing, cancellation and switching back to English. Languages omitted from a Qt engine build are explicitly skipped. The normal suite also tests candidate selection, stale-target rejection and locale persistence with deterministic engine doubles. A disposable `gazegrid-local-ime-check` Docker image can be built from `tools/local-ime.Dockerfile` using the existing `gazegrid-linux-check` base image; no host Qt installation or active desktop layout is modified.

`local-ime-ui-tests` loads the production keyboard QML and drives its gaze hit-testing and dwell timer. It covers language selection, stationary-key dwell, candidate paging and commitment, pause/navigation cancellation, focus isolation, and error reporting when the module fails. The executable forces offscreen rendering, disables desktop/tracker output, and uses a temporary profile. With `GAZE_GRID_TEST_LOCAL_IME=1`, it additionally composes and selects `你好` and `日本` through the real engines and production suggestion row. Candidate refreshes preserve consumed letter/paging targets, while candidate generation tokens still invalidate stale selections. Composition is cancelled before a pause or layout change makes the editor read-only.
