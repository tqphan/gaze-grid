# Keyboard layouts and editor

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

See [the complete example](examples/freeform.xml). Copy it into your keyboard
pack directory and select it in Settings → Keyboard after restarting. Placement can be edited visually with the keyboard editor described below.

Virtual controller output and gaze/mouse joystick keys are available from the game toolbar’s **Gamepad** key. See [controller actions and setup](game-controller.md).

Key mode can be cycled through **Tap → Repeat → Hold → Toggle** in **Settings → Gaze & dwell**.
Custom keyboard keys can use `<Action>CycleKeyMode</Action>`. In Settings > Keyboard selection, choose which modes it includes using the Tap, Repeat, Hold, and Toggle checkboxes. At least one mode must stay selected. Selected modes cycle in that order; use Save preferences to keep your choices. Save preferences also remembers the current key and button modes between app launches.
The mode covers all physical keyboard keys except modifiers (Shift, Ctrl, Alt and Super/Windows).
Hold sends a physical key-down after the dwell delay and releases when mouse or gaze
leaves the key; pause, tracking loss and layout changes also release it.
Toggle keeps a key pressed until selected again; changing modes, pausing, tracking loss
and layout changes release toggled keys (unless layout persistence is enabled). Desktop authorization is required for Hold and Toggle. Conversation keyboards retain text entry.
Repeat uses the configured repeat interval; Hold uses the destination application's
normal held-key behavior. Clicking a key taps it except in Toggle mode, where it toggles the key. Saved preferences retain the selected mode.

Individual keys can override the cycle state using `<DynamicKey mode="Hold" ...>`.
Use `Tap`, `Repeat`, `Hold`, or `Toggle`; omit `mode` to follow the cycle.
This works for keyboard keys and digital gamepad buttons.
See [fixed XML modes](game-controller.md#fixed-xml-modes) for examples.

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
pack validation. Save into your keyboard pack directory and restart Gaze Grid to
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
