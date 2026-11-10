# Host acceptance — recorded evidence

Host: Arch Linux x86-64, KDE Wayland, Qt 6.12.0. Entries below are historical results from their recorded implementation stages, not current feature descriptions or a new validation run. Later entries supersede earlier limitations; current controls are in [README](../../README.md) and outstanding work is in [the roadmap](../roadmap.md). Historical layout counts, defaults and UI descriptions are retained only as context for the recorded checks.

## Initial prototype — 2026-10-18

| Check | Result |
| --- | --- |
| Native C++/Qt build | Pass with project-local CMake 4.4.4 and Ninja 1.13.2 |
| Core tests | Dwell continuity, target changes, explicit activation, XML parsing, command ordering/whitespace and entity rejection |
| Required pack | All 22 XML files parse; 570 DynamicKey entries; all local keyboard references resolve |
| Wayland panel | Native bottom layer-shell window rendered and exited successfully; keyboard interactivity disabled |
| Portal discovery | RemoteDesktop version 2; AvailableDeviceTypes 7 |
| Authorized input session | Pass: libei keyboard/pointer devices became ready after desktop permission approval |
| Separate-process input delivery | Pass: disposable Wayland Qt receiver received `optikey` followed by Enter |
| Supplied Tobii SDK | Exact x64 library loads and reports 3.0.4.6031; no substitution |
| Tobii enumeration | Library returned zero devices; live gaze remains unverified |
| Tobii services | `tobii_engine` and `tobii_usb` reported active |
| Original repository | No rewrite files or changes added |

The separate-process test proves keystroke delivery to a focused Qt target. It does not yet establish focus preservation when physically clicking the panel, input delivery into a browser/XWayland application, or pointer-action correctness.

No privileged helper or Tobii service installation was performed. CMake/Ninja were installed into `.tools/` after system package installation could not proceed without a sudo password. The selected Tobii SDK remains in its original user-supplied location.

## Action-runner regression follow-up — 2026-10-18

The current source builds and `./build.sh` passes the core suite, including all 22 layout compilation fixtures. Added regression coverage verifies that either held Shift key records effective US punctuation in scratchpad text (for example, Shift+1 records `!`) and remains held until explicitly released. Backspace combined with either Ctrl, Alt, or Win key still sends the physical key sequence but no longer reports an ordinary single-character scratchpad deletion. Unmodified Backspace continues to report that deletion. Shortcut effects in the destination application are not inferred or synchronized into the scratchpad.

These checks use a fake output backend; no desktop-input or live Tobii acceptance was performed in this follow-up. The initial prototype descriptions above remain historical and do not certify the newer implementation's full pack compatibility.

## Preferences recovery follow-up — 2026-10-18

Added a versioned JSON preference store, explicit Save and Reset settings controls, atomic writes, and a validated backup. Regression tests pass for round-trip persistence, startup keyboard persistence, corrupt-primary recovery, preserving the backup after corruption, resetting both copies, rejecting malformed/out-of-range preferences, preserving newer-version files, and reporting filesystem write failures. Reset leaves input paused and keeps learned words and shortcut mappings separate. The former prototype QSettings file is no longer read or written.

`./build.sh` passes (23 QtTest results, including initialization and cleanup). An offscreen application smoke run with an isolated temporary configuration also exits successfully without QML errors. These automated checks do not establish assistive-input usability of the settings window or desktop/hardware acceptance.

## Pointer, XML and lifecycle follow-up — 2026-10-18

Implemented a separate pointer executor with two-point drag, timed movement/button release, repeat of completed targeted actions, cancellation, click/held-button conflict prevention, and relative direction actions matching the original 25-pixel default. Magnetic cursor and selected-anchor look-to-scroll have implementations; look-to-scroll currently covers the original default free-axis/medium-speed/50-pixel-deadzone calculation, not every original settings mode.

Added Screenshot-portal magnification with a four-times cropped selection image, coordinate remapping, cancellation and single-display checks. Five isolated fake-portal tests cover image delivery, denial, cancellation with a late response, nonlocal URI rejection and unexpected request handles. The fake service does not capture the desktop. Real KDE capture consent/focus and assistive selection remain unverified.

Dwell recovery controls now remain usable while a macro waits. Display changes pause/cancel output, and Tobii samples retain their age across worker-to-UI queuing. All 86 navigation links and back-return flags pass Controller tests. XML menu visibility and symbol margins are applied, with an explicit settings option to include hidden layouts.

The final automated run passed all three suites (47 test cases excluding setup/cleanup). All 22 layouts rendered offscreen without diagnostics; artifacts and per-layout coverage are in `runtime/render-audit/`. All 51 fixture action names now have implementation paths, but full behavioral parity remains unverified. No live Tobii probe or streaming test was performed.

See [the roadmap](../roadmap.md) for remaining work.

## Native desktop follow-up — 2026-10-18

A fresh `./run.sh --input-check` passed on the real KDE Wayland session: portal authorization produced usable libei devices; imported XML commands delivered `optikey` plus Enter to a separate receiver; an injected click on the XML `q` key preserved receiver focus and delivered `q`. The harness now stops on receiver focus loss. This adds panel-click focus evidence; browser/XWayland, live drag/scroll/magnifier and sustained-use acceptance remain pending.

## Speech lifecycle follow-up — 2026-10-18

Replaced detached `spd-say` calls with an owned libspeechd connection on a worker thread. Stop cancels only this connection's messages. Pending requests cannot speak after cancellation; submission failures close the connection and the next request reconnects. Errors are reported without exposing scratchpad text in diagnostics. Four fake-backend tests cover ownership/cleanup, connection failure, cancellation during connection establishment, and reconnect after failed submission. No audible speech was generated during these tests; live voice and cancellation acceptance remain pending.

## Prediction, visual preferences and dwell controls — 2026-10-18

Prediction now handles tabs, punctuation and prefix casing, avoids duplicate learning across repeated separators, and refuses suggestion activation while keys are held. Settings, Speak and Stop speech support dwell activation; entering Settings preserves the current pause state and ongoing output. Keyboard height and high contrast are optional, validated, persisted overrides. Minimized recovery layouts keep their imported size even when height scaling is active.

All four automated suites pass (55 test cases excluding setup/cleanup). Additional offscreen renders verified the scrollable settings window, a 125% high-contrast keyboard and unchanged minimized-panel dimensions, without QML diagnostics. These checks do not establish full gaze operation of the settings fields.

## Native pointer, magnifier and permission follow-up

The separate full-screen Wayland receiver passed targeted left click, double-click, two-point drag, scroll and right click. The double-click harness deduplicates Qt press/double-click reports using their shared event timestamp. The magnifier check passed actual KDE Screenshot capture, crop-content verification and precise click delivery after receiver focus returned. These checks use synthetic target points, not live gaze.

Optional desktop permission storage now uses private single-use restore tokens, version-gates persistence, and provides Disconnect and Forget. Five isolated RemoteDesktop tests verify default opt-out, token rotation, older portal fallback, denial and forgetting. All five automated suites passed (66 cases excluding setup/cleanup). Real remembered-session restoration and revocation remain pending. Shortcut mapping validation and corrupt-file fallback are covered; the default KDE previous-screen mapping preserves ordered modifier release, and unknown shortcuts remain explicit.

After the portal changes, native `--input-check` passed again: imported XML typing reached the separate Wayland receiver, and clicking XML q preserved receiver focus and delivered q. The complete 22-layout offscreen render audit also passed without QML diagnostics.

## Native input-method continuation — 2026-10-19

The current source builds. Core, controller and speech suites pass inside the
sandbox; screenshot and portal suites pass when allowed to create their private
D-Bus sockets. No real portal authorization was requested by these suites.

The isolated headless KWin harness passes native Unicode text (`é`, `€`, emoji),
Backspace, Return and password-field delivery to its separate Qt receiver. The
initial run exposed a startup race: the receiver enabled its field before the
input-method interface was bound, and the context received no content/commit
state. The receiver now retries field entry until text arrives. This validates
post-startup field entry, not automatic recovery of an already focused field.

CMake now generates the concrete input-method desktop entry; desktop-file-validate
passes. The entry has not been installed and the real session default remains
unchanged. Real-session application compatibility, panel placement/focus,
startup recovery and live gaze remain outstanding.

## Startup recovery and desktop registration — 2026-10-19

Supersedes the receiver retry workaround above. The backend now makes one
asynchronous KWin context re-adoption request when activation has received no
committed state after 250 ms. Output remains unavailable until protocol state
arrives; unknown field privacy remains conservative. Deactivation cancels the
pending timer, and there is no repeating recovery loop.

Both the ordinary isolated test and `python3 tests/native_input_method.py
--late-start` pass with the receiver's original field left focused. The latter
deliberately starts the keyboard one second late. Unicode, Backspace, Return and
password-field delivery all pass. The harness now uses a log file instead of a
pipe held open by descendant portal services, and requires a successful compositor
exit as well as the receiver's pass marker. All five build regression suites pass.

The validated desktop entry now launches the project `run.sh`, retaining local
preferences/cache behavior. It is installed at
`~/.local/share/applications/org.gaze_grid.gaze_grid.InputMethod.desktop`, and
`kbuildsycoca6 --noincremental` succeeded. The existing kwinrc selection remains
`/usr/share/applications/org.kde.plasma.keyboard.desktop` with mode 1.
Real-session application compatibility and gaze acceptance remain outstanding.

## Placement button feedback — 2026-10-19

Native-mode keys that target unsupported top/offset layouts now show an explicit
unavailable label, with a tooltip and accessible explanation. Click/dwell reports
the reason before starting any commands. Original XML remains unchanged.
The main bottom bar's first key is covered by controller regression checks for
native rejection and standalone navigation to the top bar. The five regression
suites and isolated delayed-start native input check pass; no QML errors were
reported by that native run. This improves feedback, not native top placement.

## Native top placement — 2026-10-19

Supersedes the top-placement restriction above. Native mode now accepts top
layouts. Moving between bottom and top recreates only the keyboard's Wayland
surface: the bottom uses input-panel-v1 and the top uses non-focus-taking
layer-shell. The input-method context and QML window are retained. The control
bar moves to the opposite edge and gaze coordinates use the selected placement.
Offset/minimized layouts remain unsupported in native mode.

All five regression suites pass. The isolated KWin test exercises the main bar's
first key in both directions, sends Unicode/Backspace/Return at the top, then
sends private-field input at the bottom. A protocol trace confirms top anchoring,
keyboard interactivity disabled, and a fresh input-panel surface on return.
The receiver checks for focus loss. Tests use command activation, not a real
mouse or gaze click; real-session mixed-DPI and live gaze acceptance are pending.

Top placement overlays application content and stays visible outside text fields.
KWin's native panel visibility/resizing behavior remains associated with bottom
placement. Restart the running OptiKey instance to load the new build.

## Repeated placement-button validation — 2026-10-19

The native integration harness now sends Qt mouse press/release events to the
rendered placement button rather than calling its controller command directly.
It checks every resulting layout across 11 transitions in a normal text field,
then another 11 in a private field. The separate receiver fails on focus loss;
Unicode, editing keys and private-field input must still arrive after switching.
Normal and deliberately delayed startup runs pass all 22 transitions.

These are local Qt mouse events on real Wayland surfaces in isolated KWin, not
physical/compositor-injected pointer events or live gaze. This validates the QML
click path and repeated surface recreation without claiming real-session mouse
or gaze acceptance. The running user-session keyboard was not restarted for this
test-only change.

## Connected tracker stream check — 2026-10-19

Host USB detected the Tobii EyeChip and both `tobii_engine` and `tobii_usb`
services were active. `./run.sh --diagnose` enumerated one device through the
selected 3.0.4.6031 SDK. A 20-second read-only harness compiled against the
current `src/TobiiInput.cpp` successfully subscribed to gaze and received 658
signals, including 657 valid samples and 648 valid points within normalized
screen bounds. The signal total includes the worker's final invalidation signal.
The harness and aggregate results are under `runtime/gaze-check/`; no individual
gaze coordinates were recorded and no desktop input was emitted.

This supersedes the earlier zero-device result. Calibration accuracy, live dwell
typing, disconnect/reconnect, and sustained-use acceptance remain unverified.

## Gaze delivery and visible feedback — 2026-10-19

The `--gaze-check` diagnostic now runs the live worker through the controller's
freshness/bounds filter and QML signal handler, with dwell disabled and output
paused. The first 20-second check received 640 valid controller and QML points,
351 inside the alphabet keyboard. This establishes delivery through the UI, not
visual alignment with the user's intended target.

Replaced the small keyboard-only indicator with a 32-pixel full-display marker
on a transparent, non-focus-taking overlay. Enabling Gaze connects Tobii;
`--gaze` enables it at startup. A second live check received 642 valid controller
and QML points. The Wayland trace confirmed the gaze overlay's full display size,
empty input region, keyboard interactivity 0, and exclusive zone -1. All five
regression suites passed. Calibration accuracy, user-confirmed marker visibility,
and live dwell typing remain pending.

## Regular controls window — 2026-10-19

Converted the controls/status bar from a reserved layer-shell strip to an
independent, decorated desktop window with wrapping controls, scrollable content,
and normal move/resize/minimize behavior. It no longer reserves desktop space or
moves when the keyboard changes edges. Closing it quits the application.
Removed fixed-edge gaze hit testing for its controls; they use pointer clicks.
Gaze keyboard selection is suppressed while that window has focus.

The build and native smoke check passed without QML diagnostics. A Wayland trace
confirmed an xdg_toplevel titled "OptiKey controls" and compositor-provided
window decorations (mode 2), with no controls layer-shell surface.

## OptiKey gaze smoothing port — 2026-10-19

Ported the original `DataFilters/KalmanFilter.cs` process-noise/gain equations
and optional weighted target-change history into `GazeSmoothing.h`. The upstream
current-process-noise replacement is retained. The port seeds from the first
valid point instead of zero and caps the exponent before overflow. Filtering
uses logical display-coordinate distances, then returns normalized positions
for all gaze consumers. Invalid/stale samples, timeouts, display changes and
setting changes reset history; invalid samples cannot be turned into valid ones.

Settings exposes levels 0–4 and target-change smoothing. This prototype defaults
to Medium (2), rather than upstream's Off (0); target-change smoothing defaults
false. Both options apply immediately and persist through the existing explicit
Save control. Legacy profiles remain valid; invalid types/ranges are rejected.

All five suites pass, including reference numerical trajectories, strength,
large jumps, off/reset behavior, controller scaling and invalid/stale/timeout
recovery, persistence/reset and legacy profile validation. Settings smoke passed
without diagnostics. A 20-second native live check delivered 630 valid filtered
points through both controller and QML, with 249 over the keyboard. These results
do not establish a preferred smoothing strength or calibration accuracy for the
user; those remain subjective live-use checks.

## Startup gaze selection fix — 2026-10-19

Removed the QML keyboard hit-test restriction on `controls.active`. The regular
controls window could take focus at launch and suppress all keyboard dwell
selection until another window became active. Keyboard hit testing now continues
independently of controls focus; explicit Pause, invalid gaze, and desktop-output
permission requirements remain enforced.

The new isolated offscreen `gaze-startup-tests` regression keeps the controls
window active, supplies synthetic gaze to a navigation key, and requires dwell
to change layouts without losing controls focus. It reproduced the timeout
before the fix and passed afterward. All six suites pass. This verifies startup
dwell navigation, not text delivery into an unfocused destination application.

## Gaze restoration from minimized keyboard — 2026-10-19

The imported minimized layouts shift a 20%-height window by 19.95% of display
height, leaving roughly zero or one visible pixel. The standalone geometry now
uses a fully visible 56-logical-pixel-high recovery tab at the same edge, with a
minimum width of 240 pixels (bounded by the display). It is labeled "Restore
keyboard" and retains the XML's 750 ms BackFromKeyboard dwell action. XML files
are unchanged; this supersedes earlier preservation of minimized geometry.

The new gaze-minimize regression reproduced the off-screen recovery failure
before the fix. It now dwells through minimize/restore at both edges, returning
to the previous layout and requiring all target points to be on-screen. All
seven suites pass. A rendered snapshot verified the visible label and contrast.

## Configurable corner restore square — 2026-10-19

Replaced the edge tab with a square, default 64 logical pixels at bottom right,
12 pixels inset. Settings exposes all four corners and sizes 40–128 pixels;
values apply immediately and use the existing Save/Reset preferences flow.
Legacy profiles use defaults. The square uses overlay-layer positioning with
an exclusive zone of -1 and explicit top/left margins, so reserved desktop-panel
space cannot shift it away from the coordinates used for gaze detection.

All seven suites passed, including minimize/restore at both keyboard edges for
all four corners and minimum/maximum square sizes, plus preference persistence.
A native KDE smoke trace confirmed a 64x64 configured surface with expected
margins and exclusive zone -1. Its captured appearance was inspected. Live user
gaze restoration remains to be confirmed.

## Progressive magnification

The earlier streaming interpretation of Live zoom was replaced with progressive magnification: a single screenshot grows from 1× to the selected zoom over a saved duration (500–5000 ms, default 1500 ms). The original immediate magnification remains available. Selection is deferred while the image moves; cancellation stops the animation. No ScreenCast session or Qt Multimedia dependency is used. Saved liveZoom choices migrate to progressiveZoom. Regression tests cover growth, final crop and coordinate mapping, settings persistence, cancellation, and rendered QML output.

## Pointer targeting by dwell

Magnifier targeting now supports gaze or pointer-hover dwell for both the initial target and final confirmation. Saved settings control whether target dwell is enabled, the start and confirmation durations (250–3000 ms), and movement tolerance (10–100 px). A progress ring follows the selected point. Confirmation starts a fresh dwell after progressive zoom finishes; invalid tracking, capture, cancellation and movement outside the tolerance reset progress. Spatial tolerance replaces the old fixed-grid gaze targeting.

Offscreen QML checks exercise the complete progressive zoom flow with synthetic gaze and pointer movement, without mouse-button events or desktop capture. Unit tests cover jitter across former grid boundaries, tracking gaps, and preference persistence/validation. These checks do not establish live tracker accuracy or actual desktop click delivery.
