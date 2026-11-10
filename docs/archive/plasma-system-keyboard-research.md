# OptiKey as Plasma's system keyboard

Research date: 2026-10-18. Historical source research retained for the native backend. Completed implementation proposals have been removed; remaining acceptance is listed below.

## Host and source baseline

Installed package metadata reports `kwin`, `plasma-workspace` and `plasma-keyboard` version **6.8.0-1**. `~/.config/kwinrc` selects `/usr/share/applications/org.kde.plasma.keyboard.desktop` under `[Wayland]`, with `VirtualKeyboardMode=1`. In this version, the modes are 0 disabled, 1 touch/tablet, 2 touch/tablet/mouse. The display-manager service points to `plasmalogin.service`; login-screen integration must therefore be investigated against Plasma Login, not assumed to use SDDM.

Downloaded and inspected official KDE **v6.8.0** source archives under `/tmp/optikey-plasma-research/`. Current master has a different keyboard framework, so its README is not an accurate description of all installed implementation details. No source files were imported into OptiKey.

## How the installed keyboard generates text, keys and symbols

Plasma Keyboard 6.8 uses Qt Virtual Keyboard internally. Its process sets `QT_IM_MODULE=qtvirtualkeyboard`, loads QML layouts and adapts the resulting Qt events to Wayland input-method-v1. This environment setting is local to the keyboard process; it is not an instruction to set every application's input module. [Startup](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/main.cpp).

| Layer | What it means |
| --- | --- |
| QML `key` | Logical Qt key identity, such as `Qt.Key_Q` |
| QML `text` | Character/string produced by the key |
| `displayText` | Visible key label; it can differ from the output |
| `alternativeKeys` | Additional characters offered by a long press |
| Style icons | Visual assets for actions such as Backspace, not text to type |

For example, the German layout defines `Qt.Key_Udiaeresis` with text `ü`, and an `s` key with alternatives `sß`. Breeze renders labels with `control.displayText` and action icons with `Kirigami.Icon`, such as `edit-clear-symbolic` for Backspace. Symbol pages are layout definitions, with fallbacks to Qt's built-in layouts where provided. [Layout example](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/layouts/de_DE/main.qml), [Breeze style](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/styles/breeze/style.qml), [Qt key properties](https://doc.qt.io/qt-6.10/qml-qtquick-virtualkeyboard-components-basekey.html).

The event adapter follows two main paths:

- **Text:** Qt text/input-method events become `commit_string(serial, text)` and, where appropriate, preedit and surrounding-text operations. This supports Unicode strings rather than requiring a US physical-key sequence for every character.
- **Nontext keys:** Qt key events are converted to XKB keysyms and sent through the input-method context. Return is deliberately treated as a key despite having a newline text representation. Physical keyboard events intercepted for composition have a separate forwarding path.

The adapter translates Qt character offsets to UTF-8 byte offsets for surrounding-text replacement and preedit. It also receives password/sensitive-field hints. [Event adapter](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/inputlisteneritem.cpp), [Protocol calls](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/inputplugin.cpp).

KWin delivers input-method text to applications through text-input-v2/v3, or its internal text context. Its implementation also has a fallback that maps text to keysyms/keycodes, using a temporary keymap for otherwise unmapped symbols. This fallback is a compatibility mechanism, not proof that every browser, terminal, game or XWayland application behaves identically. [KWin text delivery](https://github.com/KDE/kwin/blob/v6.8.0/src/inputmethod.cpp).

## What makes a keyboard the system default

The Virtual Keyboard settings module discovers application desktop entries marked `X-KDE-Wayland-VirtualKeyboard=true`. The selected desktop-file path is stored as `[Wayland] InputMethod` in `kwinrc`. KWin reads its `Exec` command, launches the process, and supplies a dedicated inherited `WAYLAND_SOCKET`. Only that designated input-method connection is allowed to bind `zwp_input_method_v1` and `zwp_input_panel_v1`. Registering a desktop entry alone therefore does not turn the current OptiKey process into an input method. [Discovery](https://github.com/KDE/kwin/blob/v6.8.0/src/kcms/virtualkeyboard/kcmvirtualkeyboard.cpp), [Launch configuration](https://github.com/KDE/kwin/blob/v6.8.0/src/main_wayland.cpp), [Interface access](https://github.com/KDE/kwin/blob/v6.8.0/src/wayland_server.cpp).

Plasma Keyboard assigns its QQuickWindow the input-panel surface role through a Qt Wayland shell integration. Its keyboard panel requests bottom-center placement. OptiKey currently uses LayerShellQt for top/bottom XML layouts and independent control/target windows. A Wayland surface cannot simultaneously have both roles. The implementation recreates the surface when switching between the bottom input panel and top layer-shell overlay. Native offset/minimized layouts remain unsupported. [Panel integration](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/inputpanelintegration.cpp), [Panel placement](https://github.com/KDE/plasma-keyboard/blob/v6.8.0/src/qwaylandinputpanelsurface.cpp).

The normal system text-input path does not need a RemoteDesktop portal session. Pointer control and persistent physical shortcuts remain separate capabilities. Current upstream development mentions KDE fake-input for modifier emulation; the installed 6.8 source inspected here uses Qt Virtual Keyboard and the input-method adapter. Do not treat master-only behavior as installed behavior.

## Remaining acceptance

The backend, native commits, sensitive-field handling, input-panel integration, desktop-entry generation, startup recovery and top/bottom transitions are implemented. Usage and isolated test commands are in [development guide](../development.md#experimental-plasma-input-method); recorded results are in [host acceptance](host-acceptance.md).

- Validate real-session discovery, startup, focus transitions, visibility and recovery with the current gaze-grid desktop entry.
- Exercise native Qt/GTK, browsers, terminals and XWayland, including physical shortcuts and pointer control alongside native text entry.
- Resolve native offset/minimized layouts and confirm live gaze and mixed-DPI behavior.
- After validation, select gaze-grid in Plasma's Virtual Keyboard settings and retain the previous selection for rollback. Historical registration does not establish that the current renamed entry is installed or selected.
- Investigate Plasma Login and screen-locker integration separately, including tracker availability and settings ownership. A user-session default does not establish lock/login-screen support.
