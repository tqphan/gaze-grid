# Virtual game controller

Open **Gamepad** from either game toolbar, or choose **Virtual Game Controller**
in the keyboard selector. Both circular joystick keys respond continuously to gaze
or mouse hover. The center 15% is a dead zone; moving farther out increases the
axis value. Leaving a joystick centers it. **Button mode** cycles independently
through **Tap → Repeat → Hold → Toggle**, with a text badge showing the current mode.
It applies to all single digital button keys, including LB, RB and the D-pad.
Tap pulses for 100 ms; Repeat taps while dwelling at the configured repeat interval;
Hold presses after dwell and releases when mouse or gaze leaves; Toggle stays pressed
until selected again. Clicks tap except in Toggle mode. **Release all**, pause,
tracking loss and changing button modes release buttons. Layout changes release buttons
unless **Preserve toggled keys between layouts** is enabled; only toggled digital
buttons persist, while momentary presses and analog axes reset. Entering conversation
mode always releases buttons.
Analog sticks and trigger axes retain their existing controls.

Custom keys can use `<Action>CycleButtonMode</Action>`. The action is also available
in Settings → Gaze & dwell. Saved preferences retain the selected button mode.
Explicit Release actions and multi-command macros retain their original behavior.

On Linux, output uses the [standard Linux gamepad mapping](https://kernel.org/doc/html/latest/input/gamepad.html)
and appears as `Gaze Grid Virtual Controller`. Games must support Linux controllers
(or use Steam Input).

On Windows, output uses [ViGEmBus](https://github.com/nefarius/ViGEmBus) and appears
as an Xbox 360 controller. Install the upstream bus driver separately. The client
library is built into gaze-grid; no separate ViGEmClient DLL is needed. Missing-driver
and connection errors appear in action status, and subsequent actions retry.
Xbox buttons, D-pad, sticks and triggers are supported. Digital trigger buttons
hold the corresponding trigger at full pressure; releasing them restores any
analog trigger value. Hat axes and D-pad buttons combine without releasing one
another. Positive screen Y maps to negative XInput stick Y.

Windows rejects Linux-only C/Z, Button1–32, Throttle, Rudder and Wheel actions.
These remain available on Linux. Neither backend implements motion sensors,
touchpads or force feedback. Windows hardware/driver acceptance remains pending;
the report mapping and lifecycle are tested with injected output.

## Device access

The Linux `uinput` module must be loaded and the current user must have write
access to `/dev/uinput`. Desktop portal authorization does not grant that access.
Use your distribution's controller-device permissions package or ask your
administrator for a session-scoped udev rule for uinput. Do not run Gaze Grid as root.
An access failure is shown in Gaze Grid's action status. The device is created on
first non-neutral output; a game may take a moment to discover it.

## Actions

Every action below works in `<Action>...</Action>` in a DynamicKey or macro.
Names are case-sensitive. Buttons use `Gamepad` + name + operation:

- Names: `A`, `B`, `X`, `Y`, `C`, `Z`, `LeftShoulder`, `RightShoulder`,
  `LeftTriggerButton`, `RightTriggerButton`, `Back`, `Start`, `Guide`,
  `LeftStick`, `RightStick`, `DpadUp`, `DpadDown`, `DpadLeft`, `DpadRight`,
  and `Button1` through `Button32`.
- Operations: `Press`, `Release`, `Toggle`, `Tap` (100 ms).
- Example: `<Action>GamepadAPress</Action><Wait>500</Wait><Action>GamepadARelease</Action>`.

Axes use `Gamepad` + axis + `:` + normalized value:

- `LeftX`, `LeftY`, `RightX`, `RightY`, `Throttle`, `Rudder`, `Wheel`: -1 to 1.
- `LeftTrigger`, `RightTrigger`: 0 to 1.
- `HatX`, `HatY`: -1, 0, 1 (fractional values round to the nearest position).
- Example: `<Action>GamepadLeftX:-0.5</Action>`; positive Y means down.
- `GamepadReset` releases every button and centers/zeros every axis.

Buttons and axes persist until released/reset. Pause, tracking loss, layout
changes, recovery and action failure reset controller state. Conversation layouts
with `SimulateKeystrokes=false` never send controller output. Desktop input
permission is not required for controller-only macros.

## Joystick keys

Place this in a keyboard's Content, using normal grid or canvas geometry:

```xml
<Joystick row="0" col="0" width="3" height="3" stick="Left">
  <Label>Joystick</Label>
</Joystick>
```

`stick` is `Left` (default) or `Right`. Joystick keys operate continuously without
dwell activation. Looking away centers the stick so users can safely select
another action. Use discrete axis actions when a stick must stay deflected while
selecting other keys.

## Fixed XML modes

Set `mode` on a `DynamicKey` to override the current cycle mode for that key:

```xml
<DynamicKey row="0" col="0" mode="Hold">
  <Label>A</Label>
  <Action>GamepadATap</Action>
</DynamicKey>
<DynamicKey row="0" col="1" mode="Toggle">
  <Label>LB</Label>
  <Action>GamepadLeftShoulderToggle</Action>
</DynamicKey>
```

Allowed values are `Tap`, `Repeat`, `Hold`, and `Toggle` (case-insensitive).
Omit the attribute to follow the cycle mode. An explicit mode takes precedence
over the button action suffix and the cycle state. Explicit Release actions,
analog axes, modifiers and macros do not accept this attribute.

The same attribute works for physical keyboard keys, for example
`<DynamicKey row="0" col="2" mode="Repeat"><Label>Space</Label><Action>Space</Action></DynamicKey>`.
Hold and Repeat apply
to dwell; clicking taps except in Toggle mode. Normal pause and release safeguards
still apply, including releasing held buttons when changing cycle modes.
