# Using Gaze Grid

## Keyboard input and composition

Keyboard input uses the system layout. The bundled keyboards retain their key positions, actions, macros and navigation. Standalone conversation layouts start the local Qt input engine automatically; choose a language in Settings > Keyboard > Input language.

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

The local scratchpad uses Qt's plain-text `TextEdit`. In standalone conversation layouts it accepts local editing, and Qt owns selection, preedit, Unicode replacement and IME commits. Only committed text is synchronized to the Controller for speech and suggestions. Desktop-control layouts retain a read-only scratchpad and do not take keyboard focus; the Plasma input-method panel also remains non-focusable. Existing gaze-key actions and personal keyboard geometry are preserved. A local Qt control cannot observe another application's IME commits, so desktop scratchpad mirroring remains a separate integration task. Offscreen `scratchpad-tests` send IME events directly to the Qt control without desktop input or layout activation.

Gaze text and editing actions in a visible standalone conversation scratchpad now route to the registered Qt control, including its cursor and selection. On-screen keys use the local input engine; Qt handles navigation, Delete and Backspace. This route never sends desktop input and does not echo text a second time through the Controller. Headless operation retains the previous text-only path.

Local composition starts automatically with Qt Virtual Keyboard installed. The input-context plugin is selected only for this application process, and Qt's separate desktop keyboard window is disabled. A transparent, disabled implementation panel initializes the engine while the gaze layout remains in place. Virtual keystrokes use the engine's virtualKeyClick API, and only committed text reaches the scratchpad. Missing engines or an unfocused editor report an error. Sign-in and Plasma input-method modes do not activate the local engine. This is separate from Windows native IMEs; Wayland deployment requires separate compositor/input-context validation.

In a standalone conversation layout, **Settings → Keyboard → Input language** lists the local engine's supported locales. Save preferences to retain the choice. Engine candidates use the existing suggestion row and previous/next suggestion keys, with no new key positions. Candidate commands include a generation token, and selection checks the current model text; changed lists, languages, pages and contexts invalidate stale dwell targets. The suggestion row shows engine candidates when available and ordinary word predictions otherwise. Paging and selection follow the displayed list. An unavailable saved locale reports an error and falls back to the engine's actual locale.

The engine's locale list is broader than the gaze input paths verified here: English, Simplified Chinese Pinyin and Japanese Roman input. Other scripts can require matching key mappings; an installed engine plugin alone does not establish gaze input support.

Candidate notifications are coalesced until the current engine update returns, avoiding reads of a partially replaced candidate list. Input is rejected while the requested language is still loading.

On Windows, ./build.ps1 and the signed release script bundle Qt Virtual Keyboard, its layout/extension plugins and data by default using platform/windows/Deploy-Qt.ps1. The selected Qt kit must contain Qt Virtual Keyboard. No --local-ime launch flag or keyboard source selection is needed.


With gaze input enabled, a connected eye tracker keeps gaze dwell active. Tracker-invalid samples or non-finite coordinates pause gaze selection; they do not switch to mouse dwell. Sample timestamps and gaps between samples do not invalidate gaze: the last valid point remains active until invalid input, disconnection, or another input reset. Mouse dwell is used when the tracker disconnects or gaze input is disabled. Settings shows the same connection-based input mode.



## Controls

On Wayland, the standalone keyboard uses the overlay layer so it remains above fullscreen applications without taking keyboard focus. This includes top and bottom layouts and the minimized restore button. Experimental KWin input-method mode retains its native bottom input panel; its layer-shell layouts use the same overlay layer.

- **Authorize** requests keyboard/pointer access through KDE's portal. The preferred backend is libei; unsupported EIS can use portal D-Bus methods in a separate session mode.
- The imported XML keyboard opens directly. **Settings** honors XML menu visibility; enable **Include layouts hidden by the XML pack** to select any of the 29 layouts; navigation keys and **Home / recover** also change layouts. Geometry, symbols, key groups, ordered macros, modifier toggles, scratchpad text, dictionary suggestions and editable platform mappings have implementations. Full behavior and appearance parity remain unverified.
- **Modifier keys** (Shift, Ctrl, Alt, and Win) cycle through off → latched → locked → off. A **Latched** badge means the modifier releases after the next text or function key; a filled highlight and **Locked** badge mean it stays held until selected again. Latched and locked modifiers carry over between keyboard layouts and survive looking away or losing gaze tracking. Pause and explicit cancellation clear both states. Explicit XML `KeyToggle` commands retain their held/released behavior.
- **Tobii** connects through the selected SDK. **Gaze** also connects automatically and enables gaze selection on the primary display, with a gaze marker drawn directly on the keyboard. Settings → Status reports whether valid on-screen gaze is arriving. The tracker is assumed calibrated to that display. Live streaming has been verified; calibration and dwell acceptance remain pending. Display changes pause input and require checking calibration before resuming.
- **Pause** releases owned input and cancels pending actions. A keyboard key containing a single `<Action>Sleep</Action>` toggles pause/resume, including while paused or a macro is running. The Pause key uses the same active highlight as Magnifier; it is highlighted while paused. Click again to resume, or look away and dwell on the key again. Dwell can activate Pause and recovery controls during a waiting macro. Move away and back before dwelling on Resume.
- Pointer actions support target selection, clicks, double-clicks, relative movement, scrolling and held buttons. **Drag** chooses a source and destination, then performs a cancellable sequence. Repeat reuses the completed target for targeted clicks and drags. Direction keys move by the original default of 25 pixels. A plain left click acts at the current desktop pointer; use gaze selection when that must remain independent of the panel.
- **Magnetic cursor** follows valid gaze while Gaze is enabled. It always clamps off-screen gaze to the nearest screen edge, independently of Clamp gaze for keyboard. Settings > Gaze & dwell > Magnetic mouse offers Absolute (default, moves directly to gaze) or Relative (Windows, moves toward gaze using relative events and actual cursor-position feedback). Save preferences keeps the selection after restart. **Look to scroll** selects an anchor, then scrolls in either axis beyond a 50-pixel deadzone, with the original default medium-speed calculation. Looking over the keyboard suppresses scrolling. The Look to scroll key stays highlighted while active. Off-screen gaze is clamped to the screen edge by default and keeps scrolling active. Settings > Look to scroll offers Clamp gaze and Cancel off-screen; cancellation takes priority when enabled. Tracking loss pauses scrolling while preserving the anchor indefinitely; fresh gaze resumes it without accumulated movement. Cancellation or pause stops it. Magnetic cursor stays enabled during tracking loss, pauses movement while gaze is invalid, and resumes automatically when valid gaze returns. Settings → Look to scroll offers speed (10–300%, default 100%), centre dead zone (10–200 pixels, default 50), and direction (both axes, vertical only, or horizontal only). Changes apply immediately; Save preferences keeps them after restart. Show zones adds a click-through overlay at the selected scroll centre, matching the dead zone and direction. Zone color and Zone opacity customize its appearance. Save preferences keeps these choices after restart. Advanced scroll-area bounds remain pending.
- **Speak** and **Stop speech** use an owned Speech Dispatcher connection on a worker thread. Stop cancels this app’s speech, and failures are reported without blocking typing. Prediction uses a bundled offline English frequency and context model, ready on first launch. It ranks completions using the preceding three words and offers next-word suggestions after a space. Optional local learning gives personal words a bounded boost; it is not required for useful defaults. Model attribution, rebuilding, and evaluation are documented in [resources/prediction/README.md](../resources/prediction/README.md). Live voice and prediction usability acceptance remain pending.

Standalone portal output uses US physical keycodes. Experimental KWin input-method mode also supports native Unicode text commits. **Magnifier** highlights when enabled, automatically remembers its state between launches, and enables magnified target selection using the desktop Screenshot portal. Set zoom from 2× to 8× (default 4×) in **Settings → Magnifier**, then save preferences. Enable **Progressive zoom** in the same settings category to grow magnification from 1× to the selected zoom over 0.5–5 seconds (default 1.5 seconds). During the animation, zoom stays focused on the initially selected target; gaze and pointer movement do not steer it. The view holds still for precise selection after the animation finishes. Leave it off for immediate magnification. Linux uses one screenshot per target, with no live screen-sharing session. On Windows, **Live refresh** offers **Off**, **15 Hz**, **30 Hz** (default), **45 Hz**, and **60 Hz**. **Off** takes one screenshot per target and stops background capture; progressive zoom can still animate that same image. Save preferences to retain the refresh choice. To use it: select a pointer action, dwell on an approximate point, approve capture if prompted, then dwell on the precise point after zoom finishes. No physical mouse click is needed. **Select pointer targets by dwell** is enabled by default in **Settings → Magnifier**, with separate start (750 ms) and confirmation (1000 ms) sliders and a movement tolerance slider (40 px). Both gaze and pointer hover work; a ring shows dwell progress. Save preferences to retain changes. Capture cancellation cancels the action. gaze-grid registers its application identity before normal portal use so supported desktops can remember screenshot approval for this app. Screenshot approval is managed by the desktop and can be revoked; it does not use the keyboard/mouse restore token. Linux maps the primary display to its EIS input region and crops it from the full-workspace screenshot, so additional monitors may remain connected. Logical monitor offsets and uniformly scaled workspace captures (including KDE mixed-DPI captures) are supported; unmatched or ambiguous input/capture layouts are rejected. Gaze remains calibrated to the primary display. Native multi-monitor KDE acceptance remains pending. Unknown actions are rejected before any part of their macro executes. Absolute pointer targeting requires an EIS region that can be mapped to the primary display. Successful XML loading or action dispatch does not establish full pack compatibility. No existing plugins or legacy settings are loaded.

## Preferences

Preferences now use a versioned `profile.json` under the application configuration directory (`runtime/config/gaze-grid/gaze-grid/` with `run.sh`). In Settings, choose **Save preferences and startup keyboard** to persist dwell time, keyboard height, keyboard theme, literal-key mode, local-learning preference, and the last selected menu-visible keyboard (or the default alphabet keyboard). Hidden toolbars and minimized layouts do not replace the startup keyboard; previously saved hidden startup layouts fall back to the default alphabet keyboard. Settings → Keyboard → Appearance offers System, Midnight, Ocean, Forest, Warm Paper, and High Contrast keyboard themes. System follows the desktop light/dark theme. The settings interface follows the desktop theme. On Linux minimized recovery layouts use a borderless restore button flush with the selected screen corner. Its width, height, and opacity (10–100%) are configurable under Settings → Keyboard → Restore button. Changes take effect immediately; saving is explicit. **Reset preferences** restores defaults and leaves input paused. Reset does not delete learned words or shortcut mappings; learned words have their own reset control.

**Settings → Gaze & dwell → Invalid gaze tolerance** pauses dwell through brief tracking loss for 0–1000 ms. The default is 500 ms; 0 ms resets immediately. With a nonzero tolerance, keyboard and point-selection dwell retain their progress without activating while gaze is invalid. Valid gaze resumes dwell; changing the target restarts it, and exceeding the tolerance resets it. Tracker disconnection still resets immediately. Use **Save preferences and startup keyboard** to retain this setting.

The same category has separate **Clamp off-screen gaze for keyboard keys** and **Clamp off-screen gaze for point selection** checkboxes. Keyboard clamping defaults to off; point-selection clamping defaults to on. Clamping moves valid off-screen coordinates to the nearest screen edge before smoothing; turning it off preserves those coordinates for the selected use. Being outside the screen does not make a tracker sample invalid or trigger invalid-gaze tolerance. Save preferences to retain these choices.

**Settings → Typing & speech → Word prediction → Remove completion space before punctuation** is enabled by default. After accepting a word with an automatically appended space, eligible punctuation replaces that space. With completion spacing enabled, `hello ` followed by `.` becomes `hello. `. One shared rule set uses the prediction language for both scratchpad and desktop typing. French retains a space before `:`; regional French conventions are not inferred. Chinese, Japanese, Thai, and CJK native punctuation do not receive an added trailing space. Closing brackets and closing quotation marks attach without adding a space; ambiguous straight double quotes preserve spacing. The symbols `& - / + = * < > |` preserve spacing. Shifted punctuation is supported. Manually typed spaces are preserved; typing another character or issuing a cursor/editing command ends the automatic replacement. Disabling removal preserves the preceding space, and disabling completion spacing suppresses the trailing space. Caret-aware desktop targets are checked again before Backspace is sent. Best-effort targets rely on the recorded keys, so unobserved external edits or caret movements can still leave that record out of sync. Save preferences to retain the settings.

**Settings → Typing & speech → Speech → Scratchpad text size** adjusts the keyboard scratchpad and its settings preview from 50% to 300% (default 100%). Changes apply immediately; use **Save preferences and startup keyboard** to retain the size after restart.

Writes are atomic and retain a validated `.bak` recovery copy. If the main profile is corrupt, startup uses that copy or defaults and reports recovery in Settings. Profiles from newer versions are preserved and cannot be overwritten without an explicit reset. This fresh format does not import the prototype's QSettings file or legacy OptiKey settings.



## Settings and gaze

Click the Gaze Grid system tray icon, or choose **Settings…** from its menu, to open
Settings. Authorize, Tobii, recovery, gaze, and speech controls are
available there in every category. There is no separate controls window.
Opening or closing Settings preserves the keyboard’s current pause state and ongoing input, including gaze editing, joystick control, and look-to-scroll. Closing Settings keeps gaze-grid
running. Choose **Quit Gaze Grid** in Settings or the tray menu to exit.
The keyboard's Settings command and `--settings` also open the same window.
Settings controls use pointer clicks; gaze hit testing remains on the keyboard
and target-selection overlay. To send text, authorize desktop output and focus
the destination text field.

Settings uses KDE desktop controls and theme colors, with a left sidebar grouping options into **Status**, **Keyboard**, **Gaze & dwell**, **Magnifier**, **Typing & speech**,
**Desktop access**, and **Shortcuts**. Live gaze, tracker, desktop-control, and last-action
messages appear under **Status**. Magnifier groups zoom behavior and target dwell controls. Keyboard groups layout, appearance, and restore-button options. Tracker, speech, and desktop connection actions live on their respective pages. Save/reset preferences remain available in every category.

**Keyboard → Appearance → Dwell progress style** offers Bottom Bar (default), Top Bar, Dual Bars, Center Bars, Opposing Bars, Fill Up, Fill Down, Fill Right, Fill Left, Outline Trace, Ring Progress, Pie Progress, Shrink Inward, and Grow Outward, with an animated preview. Changes apply immediately to keyboard keys, predictions, and the target cancel button. Use **Save preferences and startup keyboard** to retain the style after restart.

Gaze smoothing is available in **Settings → Gaze & dwell**: Off (0), Low (1), Medium (2), High
(3), and Extreme (4), plus **Smooth when changing gaze target**. Low is the
prototype's default; target-change smoothing defaults off. Changes apply
immediately; **Save preferences and startup keyboard** persists both options.
Older profiles without these fields use the same defaults.

The filter ports the original OptiKey `DataFilters/KalmanFilter.cs` equations and
optional weighted measurement history. It processes display-coordinate distances
before converting back to normalized gaze, so the marker, dwell selection, and
pointer actions share one result. Unlike upstream's zero initialization, the
first valid point seeds the estimate and history. Invalid tracking, display changes, and smoothing setting changes reset the filter.
Tracking validity checks run before smoothing; timestamps and sample gaps alone do not invalidate gaze.

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
gaze input and reconnects the eye tracker. When the tracker disconnects or gaze input is disabled, mouse dwell takes over.
Invalid samples from a connected tracker pause gaze selection; they do not switch to mouse dwell.
Switching inputs starts a fresh dwell. Reset preferences turns it off.

In **Settings → Typing & speech → Word prediction**, choose **Context-aware**
(the default), **Common words** (frequency ranking without sentence context),
or **Off**. You can separately disable suggestions after a space, or enable
**Use and learn personal words locally**. Turning personalization off stops
learning and ignores existing personal words without deleting them. Changes
apply immediately; use **Save preferences and startup keyboard** to keep them.

## Additional prediction and appearance options

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

**Keyboard > Appearance > Latch style / Lock style** offers Border Highlight, Solid Fill, Bottom Line, Soft Pulse, Expanding Ripple, Light Sweep, and Moving Border independently, with previews. Save preferences to retain these styles.

Prediction languages are selected under Settings > Typing & speech > Word prediction > Prediction language. See [multilingual prediction models](../resources/prediction/MULTILINGUAL.md) for the 18 languages, training data, and evaluation. Chinese and Thai use offline lexicon-segmented word models; Japanese retains character prediction alongside local IME conversion. Scratchpad prediction follows the caret and returns after composition ends.

**Settings > Typing & speech > Word prediction > Use best-effort prediction only** uses Gaze Grid's typed-key history for desktop prediction and completion, even when the target exposes text and caret context. It defaults to on. On Windows, accessibility timeouts and accessibility element changes within the same native focused control preserve prediction dwell; switching native controls or windows still clears typed-key context. Private fields remain excluded; scratchpad prediction is unchanged. Save preferences to retain the choice.

With best-effort-only prediction off, temporary accessibility read failures preserve prediction dwell while Windows confirms the same native focus. Accepting a prediction still requires a fresh matching text/caret snapshot.

Temporary Windows focus-query failures also preserve the displayed predictions and dwell. Acceptance waits for a successful focus check; a confirmed focus change or private field still invalidates predictions. Refreshing unchanged words and acceptance context preserves dwell.

**Settings > Typing & speech > Word prediction > Underline best-effort prediction typed text** underlines the matching typed portion of best-effort predictions. It defaults to off and does not apply to caret-aware predictions, scratchpad predictions, or IME candidates. Save preferences to retain the choice.

Magnetic mouse offers **Enable target assist where available** (off by default). On Windows, choose **Center pull**, **Edge retention**, or **Center + edges** under **Assist**. Center pull keeps the previous attraction behavior. Edge retention leaves movement inside controls unchanged and holds small boundary crossings just inside the control; looking farther away releases it. The combined option retains attraction inside and around controls and applies edge retention at their boundaries. Unsupported controls and other platforms retain normal gaze movement. Assistance is suspended while a mouse button is held. Save preferences keeps the settings after restart.

**Edge range** sets the outside boundary tolerance (0–40 pixels, default 2). **Edge inset** sets how far inside the boundary the cursor is placed (0–10 pixels, default 1). Strength and Range apply to center attraction; Target retention biases selection toward the current control. Existing profiles retain Center pull.

The **Gaze smoothing** selector also offers **Mill Mouse**. This independently implements the adaptive exponential cursor interpolation recovered from Mill Mouse 8.39. **Speed** ranges from −5 to 5. **Response** selects Standard or Fine interpolation. **Steady small moves** increases smoothing for small movements at positive Speed settings; **Jitter range** is in pixels, with Auto using the speed-dependent threshold. **Snap on large moves** bypasses smoothing beyond **Snap distance**, expressed as a percentage of the shorter screen dimension. **Cursor feedback** uses the resulting magnetic cursor position, including edge adjustments, as the next interpolation reference; Windows uses the actual cursor when available. Outside magnetic mode, smoothing follows its own previous estimate. Sampling frequency affects the response. These options are independent of the Kalman and Exponential filters and do not change existing profiles until selected.

The Mill Mouse option reproduces the recovered interpolation formulas and edge-retention principle, with explicit tuning controls. Mill Mouse's application-specific mode flags are not copied, and identical feel across every input mode has not been established.

Target assist settings: **Strength** (0?100%, default 65%), **Range** (10?200 pixels, default 40), and **Target retention** (0?40 pixels, default 8). Retention adds a preference for the current target when nearby controls compete; zero removes that preference. These settings apply immediately and are retained by Save preferences.
