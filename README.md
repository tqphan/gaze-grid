# gaze-grid — Linux and Windows

Keyboard input uses the system layout. The bundled keyboards retain their key positions, actions, macros and navigation. Standalone conversation layouts start the local Qt input engine automatically; choose a language in Settings > Keyboard > Language.

Alphabet keys now declare their physical identity explicitly, for example:

```xml
<Label>q</Label>
<ShiftDownLabel>Q</ShiftDownLabel>
<PhysicalKey>Q</PhysicalKey>
<Text>q</Text>
```

System uses PhysicalKey for translated labels, desktop typing, holds/toggles and local IME keystrokes. PhysicalKey accepts an unshifted printable US key name (such as Q, 1, SEMICOLON or SPACE) on a DynamicKey with one Text command. It does not move the key. An explicit mapping also permits a custom personal label. Existing files without PhysicalKey retain their previous behavior, including automatic mapping of matching single-character labels and text. The bundled alphabet keys have explicit mappings; punctuation and macros retain their existing definitions.

System mode currently follows the foreground application's Windows keyboard layout, or uses the Linux portal/libei injection device's XKB keymap when supplied. Windows labels support Shift, Caps Lock and AltGr without consuming dead-key state. Linux labels follow compositor modifier and active-group notifications, including external Shift, Caps Lock and AltGr changes. Multi-layout maps remain unavailable until the compositor reports a valid active group. A single-layout map can also work without those notifications, using the app's modifier state. Legacy portal output and the Plasma input-method fallback alone cannot supply a system keymap. Unsupported system text input is rejected rather than guessed. This does not add an X11 backend or macOS support.

In desktop System mode, the target application handles physical typing, dead keys and composition. On Windows, prediction reads caret context from supported Edit/RichEdit controls and UI Automation text providers. Plasma native input uses surrounding-text updates. Unsupported targets and desktop scratchpad mirroring use a best-effort record of the keys GazeGrid sends, translated through the active system layout. Accepted predictions default to physical key presses for ASCII-only text and Unicode insertion when the inserted text contains a non-ASCII character. Settings > Typing & speech > Word prediction > Unicode insertion offers Non-ASCII only (default) or Always. Unsupported controls, dead-key composition and IME conversions without exposed committed text can leave best-effort context out of sync. Conversation keyboards use the local input engine described below. Custom labels without an explicit PhysicalKey, symbol keys and macros retain their labels. Applications that expose no reliable text/caret API retain best-effort prediction; interface translations and additional physical keys remain separate work.

VS Code and other Monaco editors use best-effort prediction when their native EditContext exposes only an accessibility notice or no text. To expose editor text and caret context in VS Code, set **Editor: Accessibility Support** to **On**. UI Automation context is limited to the editor's document range, so neighboring interface text is excluded.

Windows label tracking also observes physical Shift and right Alt changes while retaining the last target layout when settings has focus. The `system-keyboard-tests` suite uses injected Win32 snapshots and translations to verify layout changes, modifiers, dead-key handling and Unicode output. Its native Windows check only translates against the thread's existing layout and verifies that its layout and keyboard state are unchanged; it never activates layouts or sends input. Tests against additional installed layouts and actual IME candidate behavior still need an isolated Windows desktop or VM.

For isolated Windows desktop acceptance, build the optional `windows-layout-acceptance` target and run it in an unlocked VM with `GAZE_GRID_TEST_WINDOWS_LAYOUTS=1`. It is excluded from the default build and CTest. It uses a separate target editor to compare labels with real physical typing under US, French, German and Russian layouts, checks Shift/Caps Lock/AltGr, pending dead keys and settings focus, then restores the original layout and Caps Lock state. Key-down events are rejected if another application takes foreground focus. Run this acceptance check when changing platform input, layout tracking or focus behavior; ordinary development uses the normal automated suites.

The local scratchpad uses Qt's plain-text `TextEdit`. In standalone conversation layouts it accepts local editing, and Qt owns selection, preedit, Unicode replacement and IME commits. Only committed text is synchronized to the Controller for speech and suggestions. Desktop-control layouts retain a read-only scratchpad and do not take keyboard focus; the Plasma input-method panel also remains non-focusable. Existing gaze-key actions and personal keyboard geometry are preserved. A local Qt control cannot observe another application's IME commits, so desktop scratchpad mirroring remains a separate integration task. Offscreen `scratchpad-tests` send IME events directly to the Qt control without desktop input or layout activation.

Gaze text and editing actions in a visible standalone conversation scratchpad now route to the registered Qt control, including its cursor and selection. On-screen keys use the local input engine; Qt handles navigation, Delete and Backspace. This route never sends desktop input and does not echo text a second time through the Controller. Headless operation retains the previous text-only path.

Local composition starts automatically with Qt Virtual Keyboard installed. The input-context plugin is selected only for this application process, and Qt's separate desktop keyboard window is disabled. A transparent, disabled implementation panel initializes the engine while the gaze layout remains in place. Virtual keystrokes use the engine's virtualKeyClick API, and only committed text reaches the scratchpad. Missing engines or an unfocused editor report an error. Sign-in and Plasma input-method modes do not activate the local engine. This is separate from Windows native IMEs; Wayland deployment requires separate compositor/input-context validation.

In a standalone conversation layout, **Settings → Keyboard → Language** lists the local engine's supported locales. Save preferences to retain the choice. Engine candidates use the existing suggestion row and previous/next suggestion keys, with no new key positions. Candidate commands include a generation token, and selection checks the current model text; changed lists, languages, pages and contexts invalidate stale dwell targets. The suggestion row shows engine candidates when available and ordinary word predictions otherwise. Paging and selection follow the displayed list. An unavailable saved locale reports an error and falls back to the engine's actual locale.

`GAZE_GRID_TEST_LOCAL_IME=1` enables optional real-engine tests. They verify English key routing, Pinyin candidate commitment of `你好`, and Japanese Roman input producing `日本`, editing, cancellation and switching back to English. Languages omitted from a Qt engine build are explicitly skipped. The normal suite also tests candidate selection, stale-target rejection and locale persistence with deterministic engine doubles. A disposable `gazegrid-local-ime-check` Docker image can be built from `tools/local-ime.Dockerfile` using the existing `gazegrid-linux-check` base image; no host Qt installation or active desktop layout is modified.

`local-ime-ui-tests` loads the production keyboard QML and drives its gaze hit-testing and dwell timer. It covers language selection, stationary-key dwell, candidate paging and commitment, pause/navigation cancellation, focus isolation, and error reporting when the module fails. The executable forces offscreen rendering, disables desktop/tracker output, and uses a temporary profile. With `GAZE_GRID_TEST_LOCAL_IME=1`, it additionally composes and selects `你好` and `日本` through the real engines and production suggestion row. Candidate refreshes preserve consumed letter/paging targets, while candidate generation tokens still invalidate stale selections. Composition is cancelled before a pause or layout change makes the editor read-only.

The engine's locale list is broader than the gaze input paths verified here: English, Simplified Chinese Pinyin and Japanese Roman input. Other scripts can require matching key mappings; an installed engine plugin alone does not establish gaze input support.

Candidate notifications are coalesced until the current engine update returns, avoiding reads of a partially replaced candidate list. Input is rejected while the requested language is still loading.

On Windows, ./build.ps1 and the signed release script bundle Qt Virtual Keyboard, its layout/extension plugins and data by default using platform/windows/Deploy-Qt.ps1. The selected Qt kit must contain Qt Virtual Keyboard. No --local-ime launch flag or keyboard source selection is needed.


With gaze input enabled, a connected eye tracker keeps gaze dwell active. Tracker-invalid samples or non-finite coordinates pause gaze selection; they do not switch to mouse dwell. Sample timestamps and gaps between samples do not invalidate gaze: the last valid point remains active until invalid input, disconnection, or another input reset. Mouse dwell is used when the tracker disconnects or gaze input is disabled. Settings shows the same connection-based input mode.

On Windows with Qt 6.12 LTS MinGW installed in C:\Qt, double-click **run.cmd** to build, test, and launch. In PowerShell, use `.\build.ps1 -Run`. The script detects a Stream Engine Windows SDK beside this checkout; otherwise it uses mouse dwell. You can also pass `-TobiiSdk C:\SDK\StreamEngine` for tracker support. See [Windows instructions](docs/windows.md) for MSVC and signed releases.

The trusted Windows installation enables startup before sign-in by default. Uncheck **Start gaze-grid before Windows sign-in** in setup or pass `GAZE_GRID_SIGN_IN=0` for a silent installation to turn it off. Unsigned installations keep sign-in startup disabled, and repairs preserve the selected choice. In **Settings → Keyboard → Windows sign-in**, clear and select **Use this keyboard at sign-in** to copy your current installed keyboard and gaze preferences before entering your password. Prediction is disabled there. Administrator approval is required; actual sign-in and Tobii hardware acceptance remain pending. See the [sign-in setup notes](docs/windows.md#keyboard-before-windows-sign-in).

Windows runs one keyboard instance per desktop. The normal instance releases eye tracking and suspends input while the login/UAC desktop is active. Login defaults enable gaze without requiring copied preferences, preserve an explicit gaze-off choice, and retry if Tobii is not ready. The login keyboard displays connection errors and retry timing.

Windows app integration, Linux/Windows CI, and signed UIAccess MSI packaging are
configured. See [Windows build and release instructions](docs/windows.md) for
signing setup, SDK options, and remaining native acceptance checks.

Qt 6 Quick/QML and C++20 implementation in its own sibling project. The original OptiKey repository is unchanged. This is the initial integration prototype, not the completed rewrite.

The application is named gaze-grid; the build target is `gaze-grid`, with desktop ID `org.gaze_grid.gaze_grid`. Runtime overrides use the `GAZE_GRID_` prefix.

## Build and run on this machine

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
cd /home/q/Downloads/OptiKey-Pro
./build.sh
./run.sh
```

CMake and Ninja are installed locally in `.tools/`. The build uses the host Qt 6.12 or newer development packages, layer-shell-qt, libei and libspeechd (Speech Dispatcher). Settings also uses the KDE Kirigami and QQC2 desktop-style QML modules, with Qt Widgets providing native desktop control styling. No system helper was installed. `build.sh` defaults to an optimized `RelWithDebInfo` build with debugging symbols and runs the test suite. Use `./build.sh -DCMAKE_BUILD_TYPE=Debug` for an unoptimized debugging build. Runtime settings and caches stay under `runtime/` when using `run.sh`.

The default SDK is the exact user-supplied `stream_engine_linux_3.0.4.6031` directory. Its headers are used for compilation and its x64 shared library is loaded by absolute path at runtime. The sibling 4.24 SDK is not used. SDK files are not copied into this project. The CMake cache variable `TOBII_SDK` allows relocating the same SDK.

The Tobii worker polls subscribed callbacks approximately every 8 ms and manually synchronizes the tracker clock every 30 seconds. Synchronization is also performed on connection and reconnection; temporary synchronization failures retry after one second. All tracker processing stays on the worker thread.

## Current controls and implementation

On Wayland, the standalone keyboard uses the overlay layer so it remains above fullscreen applications without taking keyboard focus. This includes top and bottom layouts and the minimized restore button. Experimental KWin input-method mode retains its native bottom input panel; its layer-shell layouts use the same overlay layer.

- **Authorize** requests keyboard/pointer access through KDE's portal. The preferred backend is libei; unsupported EIS can use portal D-Bus methods in a separate session mode.
- The imported XML keyboard opens directly. **Settings** honors XML menu visibility; enable **Include layouts hidden by the XML pack** to select any of the 29 layouts; navigation keys and **Home / recover** also change layouts. Geometry, symbols, key groups, ordered macros, modifier toggles, scratchpad text, dictionary suggestions and editable platform mappings have implementations. Full behavior and appearance parity remain unverified.
- **Modifier keys** (Shift, Ctrl, Alt, and Win) cycle through off → latched → locked → off. A **Latched** badge means the modifier releases after the next text or function key; a filled highlight and **Locked** badge mean it stays held until selected again. Latched and locked modifiers carry over between keyboard layouts and survive looking away or losing gaze tracking. Pause and explicit cancellation clear both states. Explicit XML `KeyToggle` commands retain their held/released behavior.
- **Tobii** connects through the selected SDK. **Gaze** also connects automatically and enables gaze selection on the primary display, with a gaze marker drawn directly on the keyboard. Settings → Status reports whether valid on-screen gaze is arriving. The tracker is assumed calibrated to that display. Live streaming has been verified; calibration and dwell acceptance remain pending. Display changes pause input and require checking calibration before resuming.
- **Pause** releases owned input and cancels pending actions. A keyboard key containing a single `<Action>Sleep</Action>` toggles pause/resume, including while paused or a macro is running. The Pause key uses the same active highlight as Magnifier; it is highlighted while paused. Click again to resume, or look away and dwell on the key again. Dwell can activate Pause and recovery controls during a waiting macro. Move away and back before dwelling on Resume.
- Pointer actions support target selection, clicks, double-clicks, relative movement, scrolling and held buttons. **Drag** chooses a source and destination, then performs a cancellable sequence. Repeat reuses the completed target for targeted clicks and drags. Direction keys move by the original default of 25 pixels. A plain left click acts at the current desktop pointer; use gaze selection when that must remain independent of the panel.
- **Magnetic cursor** follows valid gaze while Gaze is enabled. It always clamps off-screen gaze to the nearest screen edge, independently of Clamp gaze for keyboard. Settings ? Gaze & dwell ? Magnetic mouse offers Absolute (default, moves directly to gaze) or Relative (Windows, moves toward gaze using relative events and actual cursor-position feedback). Save preferences keeps the selection after restart. **Look to scroll** selects an anchor, then scrolls in either axis beyond a 50-pixel deadzone, with the original default medium-speed calculation. Looking over the keyboard suppresses scrolling. The Look to scroll key stays highlighted while active. Off-screen gaze is clamped to the screen edge by default and keeps scrolling active. Settings ? Look to scroll offers Clamp gaze and Cancel off-screen; cancellation takes priority when enabled. Tracking loss pauses scrolling while preserving the anchor indefinitely; fresh gaze resumes it without accumulated movement. Cancellation or pause stops it. Magnetic cursor stays enabled during tracking loss, pauses movement while gaze is invalid, and resumes automatically when valid gaze returns. Settings → Look to scroll offers speed (10–300%, default 100%), centre dead zone (10–200 pixels, default 50), and direction (both axes, vertical only, or horizontal only). Changes apply immediately; Save preferences keeps them after restart. Show zones adds a click-through overlay at the selected scroll centre, matching the dead zone and direction. Zone color and Zone opacity customize its appearance. Save preferences keeps these choices after restart. Advanced scroll-area bounds remain pending.
- **Speak** and **Stop speech** use an owned Speech Dispatcher connection on a worker thread. Stop cancels this app’s speech, and failures are reported without blocking typing. Prediction uses a bundled offline English frequency and context model, ready on first launch. It ranks completions using the preceding three words and offers next-word suggestions after a space. Optional local learning gives personal words a bounded boost; it is not required for useful defaults. Model attribution, rebuilding, and evaluation are documented in [resources/prediction/README.md](resources/prediction/README.md). Live voice and prediction usability acceptance remain pending.

Standalone portal output uses US physical keycodes. Experimental KWin input-method mode also supports native Unicode text commits. **Magnifier** highlights when enabled, automatically remembers its state between launches, and enables magnified target selection using the desktop Screenshot portal. Set zoom from 2× to 8× (default 4×) in **Settings → Magnifier**, then save preferences. Enable **Progressive zoom** in the same settings category to grow magnification from 1× to the selected zoom over 0.5–5 seconds (default 1.5 seconds). During the animation, zoom stays focused on the initially selected target; gaze and pointer movement do not steer it. The view holds still for precise selection after the animation finishes. Leave it off for immediate magnification. This uses one screenshot per target, with no live screen-sharing session. To use it: select a pointer action, dwell on an approximate point, approve capture if prompted, then dwell on the precise point after zoom finishes. No physical mouse click is needed. **Select pointer targets by dwell** is enabled by default in **Settings → Magnifier**, with separate start (750 ms) and confirmation (1000 ms) sliders and a movement tolerance slider (40 px). Both gaze and pointer hover work; a ring shows dwell progress. Save preferences to retain changes. Capture cancellation cancels the action. gaze-grid registers its application identity before normal portal use so supported desktops can remember screenshot approval for this app. Screenshot approval is managed by the desktop and can be revoked; it does not use the keyboard/mouse restore token. Linux maps the primary display to its EIS input region and crops it from the full-workspace screenshot, so additional monitors may remain connected. Logical monitor offsets and uniformly scaled workspace captures (including KDE mixed-DPI captures) are supported; unmatched or ambiguous input/capture layouts are rejected. Gaze remains calibrated to the primary display. Native multi-monitor KDE acceptance remains pending. Unknown actions are rejected before any part of their macro executes. Absolute pointer targeting requires an EIS region that can be mapped to the primary display. Successful XML loading or action dispatch does not establish full pack compatibility. No existing plugins or legacy settings are loaded.

## Diagnostics and tests

Preferences now use a versioned `profile.json` under the application configuration directory (`runtime/config/gaze-grid/gaze-grid/` with `run.sh`). In Settings, choose **Save preferences and startup keyboard** to persist dwell time, keyboard height, keyboard theme, literal-key mode, local-learning preference, and the last selected menu-visible keyboard (or the default alphabet keyboard). Hidden toolbars and minimized layouts do not replace the startup keyboard; previously saved hidden startup layouts fall back to the default alphabet keyboard. Settings → Keyboard → Appearance offers System, Midnight, Ocean, Forest, Warm Paper, and High Contrast keyboard themes. System follows the desktop light/dark theme. The settings interface follows the desktop theme. On Linux minimized recovery layouts use a borderless restore button flush with the selected screen corner. Its width, height, and opacity (10–100%) are configurable under Settings → Keyboard → Restore button. Changes take effect immediately; saving is explicit. **Reset preferences** restores defaults and leaves input paused. Reset does not delete learned words or shortcut mappings; learned words have their own reset control.

**Settings → Gaze & dwell → Invalid gaze tolerance** pauses dwell through brief tracking loss for 0–1000 ms. The default is 500 ms; 0 ms resets immediately. With a nonzero tolerance, keyboard and point-selection dwell retain their progress without activating while gaze is invalid. Valid gaze resumes dwell; changing the target restarts it, and exceeding the tolerance resets it. Tracker disconnection still resets immediately. Use **Save preferences and startup keyboard** to retain this setting.

The same category has separate **Clamp off-screen gaze for keyboard keys** and **Clamp off-screen gaze for point selection** checkboxes. Keyboard clamping defaults to off; point-selection clamping defaults to on. Clamping moves valid off-screen coordinates to the nearest screen edge before smoothing; turning it off preserves those coordinates for the selected use. Being outside the screen does not make a tracker sample invalid or trigger invalid-gaze tolerance. Save preferences to retain these choices.

**Settings → Typing & speech → Word prediction → Remove completion space before punctuation** is enabled by default. After accepting a word with an automatically appended space, eligible punctuation replaces that space. With completion spacing enabled, `hello ` followed by `.` becomes `hello. `. One shared rule set uses the prediction language for both scratchpad and desktop typing. French retains a space before `:`; regional French conventions are not inferred. Chinese, Japanese, Thai, and CJK native punctuation do not receive an added trailing space. Closing brackets and closing quotation marks attach without adding a space; ambiguous straight double quotes preserve spacing. The symbols `& - / + = * < > |` preserve spacing. Shifted punctuation is supported. Manually typed spaces are preserved; typing another character or issuing a cursor/editing command ends the automatic replacement. Disabling removal preserves the preceding space, and disabling completion spacing suppresses the trailing space. Caret-aware desktop targets are checked again before Backspace is sent. Best-effort targets rely on the recorded keys, so unobserved external edits or caret movements can still leave that record out of sync. Save preferences to retain the settings.

**Settings → Typing & speech → Speech → Scratchpad text size** adjusts the keyboard scratchpad and its settings preview from 50% to 300% (default 100%). Changes apply immediately; use **Save preferences and startup keyboard** to retain the size after restart.

Writes are atomic and retain a validated `.bak` recovery copy. If the main profile is corrupt, startup uses that copy or defaults and reports recovery in Settings. Profiles from newer versions are preserved and cannot be overwritten without an explicit reset. This fresh format does not import the prototype's QSettings file or legacy OptiKey settings.

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

See `docs/host-acceptance.md` for observed results and outstanding gates, and `docs/OptiKey-Pro-modernization-plan.md` for the agreed decisions.

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

Click the gaze-grid system tray icon, or choose **Settings…** from its menu, to open
Settings. Authorize, Tobii, recovery, gaze, and speech controls are
available there in every category. There is no separate controls window.
Opening or closing Settings preserves the keyboard’s current pause state and ongoing input, including gaze editing, joystick control, and look-to-scroll. Closing Settings keeps gaze-grid
running. Choose **Quit OptiKey** in Settings or the tray menu to exit.
The keyboard's Settings command and `--settings` also open the same window.
Settings controls use pointer clicks; gaze hit testing remains on the keyboard
and target-selection overlay. To send text, authorize desktop output and focus
the destination text field.

Settings uses KDE desktop controls and theme colors, with a left sidebar grouping options into **Status**, **Keyboard**, **Gaze & dwell**, **Magnifier**, **Typing & speech**,
**Desktop access**, and **Shortcuts**. Live gaze, tracker, desktop-control, and last-action
messages appear under **Status**. Magnifier groups zoom behavior and target dwell controls. Keyboard groups layout, appearance, and restore-button options. Tracker, speech, and desktop connection actions live on their respective pages. Save/reset preferences remain available in every category.

**Keyboard → Appearance → Dwell progress style** offers Bottom bar (default), Top bar, Fill bottom to top, Fill top to bottom, Fill left to right, Fill right to left, Outline, Ring, Pie, Collapse, and Expand, with an animated preview. Changes apply immediately to keyboard keys, predictions, and the target cancel button. Use **Save preferences and startup keyboard** to retain the style after restart.

Gaze smoothing is available in **Settings → Gaze & dwell**: Off (0), Low (1), Medium (2), High
(3), and Extreme (4), plus **Smooth when changing gaze target**. Low is the
prototype's default; target-change smoothing defaults off. Changes apply
immediately; **Save preferences and startup keyboard** persists both options.
Older profiles without these fields use the same defaults.

The filter ports the original OptiKey `DataFilters/KalmanFilter.cs` equations and
optional weighted measurement history. It processes display-coordinate distances
before converting back to normalized gaze, so the marker, dwell selection, and
pointer actions share one result. Unlike upstream's zero initialization, the
first valid point seeds the estimate and history. Invalid/stale tracking, display
changes, and smoothing setting changes reset the filter. Tracking validity and
freshness checks still run before smoothing.

When minimized in standalone mode, look at the **↗ restore button** for 750 ms
to bring the keyboard back. Settings offers **Restore button corner** and
**Restore button width** and **Restore button height** (40–128 logical pixels each). The default is 64 × 64 pixels at
the bottom left, flush with the screen edges. Changes apply immediately;
use Save preferences to retain them. The square stays fully on-screen and
ignores reserved panel space so its displayed position matches gaze detection.
Imported XML files are unchanged.

Gaze indicator appearance is configurable in **Settings → Gaze & dwell**: diameter
(4–120 px), opacity (5–100%), and a solid RGB color (`#RRGGBB`). The default is
a solid blue circle, 18 px at full opacity. Optional **Morph with gaze acceleration
and direction** stretches it along gaze movement, with bounded deformation based
on acceleration. It returns to a circle at rest and resets after tracking loss.
These visual effects do not change gaze targeting. Use **Save preferences and
startup keyboard** to retain changes.

The gaze indicator is drawn directly on the keyboard and clipped at its edges,
including the minimized restore button. It hides when looking outside the keyboard
or when the keyboard is hidden. Size, opacity, color, and morphing remain configurable.

**Settings → Keyboard → Temporarily hide keyboard during mouse actions** hides
the panel from the start of a mouse action through target selection and execution.
The keyboard returns on completion, failure, or cancellation. Target selection
and its Cancel button remain available. Continuous magnetic cursor and look-to-scroll
modes restore the keyboard after activation so their controls remain accessible.
The option defaults off and is retained with Save preferences.

**Click duration** in Keyboard settings holds each mouse click for 0-250 ms in
25 ms steps (default 0 ms). It applies to left, middle, right, and both clicks of
a double-click. Save preferences retains the value. Drag and hold actions are unchanged.

**Transition** selects Instant Switch (default), Soft Fade, Gentle Lift,
Directional Slide, Subtle Zoom, or Row Reveal when switching keyboards.
**Duration** sets the animation time from 50-1000 ms (default 180 ms).
Keys remain inactive during animations. Save preferences retains both settings.

Enable **Settings → Gaze & dwell → Cycle modifiers while hovering** to advance
Shift, Ctrl, Alt, and Win at the configured repeat rate without looking away. The option
defaults off, applies to modifier keys only, and is retained with Save preferences.
Explicit KeyToggle modifier keys retain their two-state toggle behavior.

Use **Key mode: Repeat** or an XML `mode="Repeat"` override to repeat keyboard
keys while dwelling. Tap fires once until mouse or gaze leaves and returns.

**Repeat rate** sets the interval between repeat activations from 100–2000 ms
(10–0.5 repeats per second), default 750 ms. It applies to keyboard and gamepad Repeat mode, XML Repeat overrides,
and modifier cycling. The initial activation still uses
Keyboard dwell. Each repeat waits for the previous action to finish.

Existing restore-button size preferences initialize both width and height to the
previous square size. New width and height values save independently.

**Enable gaze input** saves automatically. When enabled, the next launch restores
gaze input and reconnects the eye tracker. When no valid gaze is available, mouse
dwell takes over for keys, predictions, and target selection. Valid gaze automatically
takes priority again; switching inputs starts a fresh dwell. Reset preferences turns it off.

In **Settings → Typing & speech → Word prediction**, choose **Context-aware**
(the default), **Common words** (frequency ranking without sentence context),
or **Off**. You can separately disable suggestions after a space, or enable
**Use and learn personal words locally**. Turning personalization off stops
learning and ignores existing personal words without deleting them. Changes
apply immediately; use **Save preferences and startup keyboard** to keep them.

## Freely positioned keys

XML keyboards can replace `<Grid>` with `<Canvas><Width>100</Width><Height>100</Height></Canvas>`.
On a canvas, every `DynamicKey`, `Scratchpad`, and `SuggestionRow` specifies
`X`, `Y`, `Width`, and `Height`, including decimal values:

```xml
<DynamicKey X="12.5" Y="20" Width="18" Height="9.5">
  <Label>A</Label><Text>a</Text>
</DynamicKey>
```

Coordinates start at the canvas's top-left and scale with the keyboard window.
A 100 × 100 canvas makes each unit one percent of the window; any positive canvas
size is supported. Set the keyboard's top-level `Width` and `Height` to `100%`
(and keyboard height scaling to 100%) to place keys across the screen.
Each rectangle must have positive dimensions and fit inside the canvas.
Keys may overlap; the last item in XML is drawn on top and receives targeting.
Empty areas do not activate keys. Key contents are clipped to their rectangles.
`SuggestionRow` accepts `Count` (1–64, default 4), dividing its width equally.
Grid layouts retain their existing cell sizing and automatic placement.

See [the complete example](docs/examples/freeform.xml). Copy it into your keyboard
pack directory and select it in Settings → Keyboard after restarting. Placement can be edited visually with the keyboard editor described below.

Virtual controller output and gaze/mouse joystick keys are available from the game toolbar’s **Gamepad** key. See [controller actions and setup](docs/game-controller.md).

Key mode can be cycled through **Tap → Repeat → Hold → Toggle** in **Settings → Gaze & dwell**.
Custom keyboard keys can use `<Action>CycleKeyMode</Action>`. In Settings > Keyboard selection, choose which modes it includes using the Tap, Repeat, Hold, and Toggle checkboxes. At least one mode must stay selected. Selected modes cycle in that order; use Save preferences to keep your choices. Save preferences also remembers the current key and button modes between app launches.
The mode covers all physical keyboard keys except modifiers (Shift, Ctrl, Alt and Super/Windows).
Hold sends a physical key-down after the dwell delay and releases when mouse or gaze
leaves the key; pause, tracking loss and layout changes also release it.
Toggle keeps a key pressed until selected again; changing modes, pausing, tracking loss
and layout changes release toggled keys (unless layout persistence is enabled). Desktop authorization is required for Hold and Toggle. Conversation keyboards retain text entry.
Repeat uses the configured repeat interval; Hold uses the destination application's
normal held-key behavior. Clicking a key taps it except in Toggle mode, where it toggles the key. The mode starts at Tap each launch.

Individual keys can override the cycle state using `<DynamicKey mode="Hold" ...>`.
Use `Tap`, `Repeat`, `Hold`, or `Toggle`; omit `mode` to follow the cycle.
This works for keyboard keys and digital gamepad buttons.
See [fixed XML modes](docs/game-controller.md#fixed-xml-modes) for examples.

In Settings > Conversations > Profiles, create and select named
profiles for the conversation keyboard. Each profile holds up to 12 items with a
key label and a separate phrase to speak. Use Apply after editing an item;
profile switches apply immediately. Use Open keyboard from this panel
to use the keys. Save preferences to keep profiles and the selection after restarting.

Custom keys can use `<Label>headrest</Label><Action>SpeakLabel</Action>` to speak
their label directly without entering text or changing the scratchpad. Leading and
trailing label whitespace is ignored. `<Action>Speak</Action>` speaks the scratchpad.

Enable **Settings → Gaze & dwell → Preserve toggled keys between layouts** to keep
toggled keyboard keys and digital gamepad buttons pressed when navigating layouts.
The setting defaults off and is stored with Save preferences. Hover-held keys,
momentary button presses and analog axes are released or centered on layout changes.
Pause, tracking loss, mode changes and entering conversation mode still release toggles.


## Key group dwell time

All bundled keyboards categorize selectable keys as Alphabet, Numbers, Punctuations, Modifiers, Functions, Predictions, Conversations, Navigation, Editing, Mouse, Gamepad, or Others. **Settings → Gaze & dwell** has a dwell slider for each category (250–2500 ms). Changes apply immediately across keyboards; **Save preferences and startup keyboard** retains them. The All groups slider sets Keyboard dwell and every category together; individual category sliders can then be adjusted separately. Until a category slider is changed, it follows Keyboard dwell. Reset preferences clears category overrides. Predictions applies to prediction slots; Conversations applies to spoken phrase keys.

An individual key's XML `CompletionTimes` takes priority over its category slider. Category sliders override group XML timing; without a slider override, group XML timing and then Keyboard dwell apply. Repeat rate remains separate.

Set `CompletionTimes` in milliseconds on a keyboard's `KeyGroup` to share dwell timing:

```xml
<KeyGroup Name="Slow" CompletionTimes="1200" />
```

Inside each member key, add `<KeyGroup>Slow</KeyGroup>`. Groups named `All` or `Default` apply to every key automatically. Later matching groups in XML order take priority; a key's own `CompletionTimes` overrides all groups. If the resulting value is missing, zero, or negative, the global dwell setting applies. As with individual keys, only the first comma-separated value is used. Repeat timing continues to use the global repeat interval.

## Keyboard editor

Open **Keyboard editor…** from the tray menu, or launch it directly:

```sh
./run.sh --editor
./run.sh --editor docs/examples/freeform.xml
```

Create a canvas keyboard or open an existing grid/canvas XML layout. Select and
drag keys to move them; edit X/Y and width/height to resize. Grid movement snaps
to cells and rejects overlaps. Add, duplicate, and delete keys from the toolbar.
Edit labels and behavior using the properties panel, then click
**Apply key changes**. **Add step** offers plain text, physical keys, application/
mouse/gamepad actions, keyboard navigation, waits, and held/released keys.
**Add shortcut…** provides Ctrl/Alt/Shift/Super checkboxes and a key picker;
press/release steps are generated automatically. Reorder or remove steps with
the list controls. **Appearance** provides an icon picker and Shift label;
**Position & size** contains precise geometry. No XML editing is required.
While properties are pending, the canvas waits for Apply or Save. Undo/redo covers
applied changes. The preview shows key geometry, labels, and icons; it does not
execute commands.
It is a layout preview, not a full rendering of the live keyboard theme.

Save validates geometry using the application's layout parser and writes atomically.
Existing metadata, attributes, command order, and text whitespace are retained;
XML formatting may change. Command behavior and navigation destinations still need
pack validation. Save into your keyboard pack directory and restart OptiKey to
select the layout in Settings. Use **Save as…** to keep the original layout.
The editor is a separate desktop window and does not request input authorization.

Keyboard XML supports `Opacity` (the whole key) and `ForegroundOpacity` (labels and symbols) on keys and key groups, using values from 0 to 1. Key attributes override group values. For example, `<KeyGroup Name="Default" Opacity="0.8" ForegroundOpacity="0.75" />`. Translucent keys reveal the desktop; the panel backdrop is omitted when a layout contains translucent keys. The minimized restore button continues to use its dedicated opacity setting.

Canvas layouts support `<Action>MoveModeToggle</Action>` and `<Action>SizeModeToggle</Action>`. Open **size-move-demo** and select **Move mode** or **Resize mode**. Dwell on a key to pick it up, then look at its new center (move) or bottom-right corner (resize). An outline previews the result; hold your gaze steady to place it. Mouse hover dwell works the same way. Move away after placement before selecting again. Select the active mode key to leave editing, or the other mode key to switch modes and cancel the pending edit. Mouse dragging also works in either mode. Items stay within the canvas and may overlap. Ordinary input is suppressed while editing. Changes are temporary by default. Under **Settings → Keyboard**, use **Save current positions and sizes to XML** to make the current geometry permanent, or enable **Save move/resize changes to original XML** to save each completed move or resize. This writes the original keyboard XML, preserving labels, actions, and other settings (XML formatting may change). Write failures are shown there and leave the changes temporary. If the XML was edited externally, reload it before saving. Automatic saving is scoped to the current keyboard session; add `<PersistKeyGeometry>true</PersistKeyGeometry>` directly inside `<Keyboard>` to enable it whenever that keyboard opens.

Background transparency and click-through are configured per keyboard in XML, not in global preferences. Add these elements directly inside `<Keyboard>`:

```xml
<TransparentBackground>true</TransparentBackground>
<BackgroundClickThrough>true</BackgroundClickThrough>
```

The **Size and move demo** enables both. Omit an element or set it to `false` to leave that option off. Existing key-opacity and minimized restore transparency still apply. Click-through applies only when the background is transparent; keys retain their input areas, including translucent keys. Moving or resizing keys temporarily restores input across the canvas. Switching keyboards uses the new keyboard's XML settings.

To activate overlapping keys by gaze or mouse hover dwell, add this per-keyboard option directly inside `<Keyboard>`:

```xml
<ActivateOverlappingKeys>true</ActivateOverlappingKeys>
```

This option applies only to Canvas layouts and defaults to `false`; Grid layouts ignore it. Each overlapping `DynamicKey` keeps its own dwell time and progress indicator. Keys completing dwell together activate modifiers first, then other keys in XML order. Actions run sequentially, preserving each key's macro and modifier behavior (a latched modifier still applies to the next ordinary key). Repeat, Hold, and Toggle modes remain available; held keys release independently when you leave their area. Leaving a key cancels its pending activation. Pausing, losing tracking, switching keyboards, or entering move/resize mode cancels pending overlap activations. Pause and mode-switch keys take priority over other overlapping keys. Joysticks and prediction slots retain their exclusive interaction when topmost, and mouse clicks still select the topmost key. Move/resize mode continues to select one item at a time.

An alternative **Exponential with saccade snap** filter is available under
**Settings > Gaze & dwell > Filter type**. It applies
`filtered += gain * (raw - filtered)` and snaps directly to the raw point when
its Euclidean distance from the filtered point exceeds the saccade threshold.
Gain defaults to 0.20 and distance to 0.10 in normalized screen coordinates;
these are adjustable and saved with preferences. Smaller gains smooth more.
This option uses its own controls independently of the Kalman strength and
optional target-change history. Existing profiles continue using Kalman.

Under **Settings > Typing & speech > Word prediction**, **Show the word I'm typing**
offers **Always, as the first suggestion**, **Only when there are no predictions**
(the default), and **Never**. The typed word preserves its exact
capitalization, including names and words absent from the dictionary. Selecting
it keeps the word and applies the existing space-after-completion setting.
Save preferences to retain the choice. Next-word predictions are unchanged.

**Keyboard ? Appearance ? Latch style / Lock style** offers Border Highlight, Solid Fill, Bottom Line, Soft Pulse, Expanding Ripple, Light Sweep, and Moving Border independently, with previews. Save preferences to retain these styles.

Prediction languages are selected under Settings > Typing & speech > Word prediction > Language. See [multilingual prediction models](resources/prediction/MULTILINGUAL.md) for the 18 languages, training data, and evaluation. Chinese and Thai use offline lexicon-segmented word models; Japanese retains character prediction alongside local IME conversion. Scratchpad prediction follows the caret and returns after composition ends.
