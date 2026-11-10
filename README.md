# Gaze Grid

Gaze Grid is a gaze- and mouse-dwell keyboard for Windows and Linux. It supports
XML keyboard layouts, desktop keyboard and pointer control, conversation phrases,
offline multilingual prediction, speech, magnification, and virtual gamepad output.
It uses Qt 6 Quick/QML and C++20.

## Build and run

On Windows, install Qt 6.12 LTS MinGW x64 and its matching compiler, CMake, and
Ninja under `C:\Qt`, then double-click **run.cmd** or run:

```powershell
.\build.ps1 -Run
```

For an isolated preview with separate settings and no desktop output or eye tracking,
double-click **test-local.cmd**. See [development and diagnostics](docs/development.md).
Signed installations, MSVC builds, Tobii setup, and startup options are covered in
[Windows instructions](docs/windows.md).

On Linux/KDE Wayland, install Qt 6.12+ development packages, LayerShellQt, libei,
libspeechd, and the KDE QML dependencies described in the development guide, then run:

```sh
./build.sh
./run.sh
```

Eye tracking requires matching Tobii SDK headers at build time and a compatible
runtime and calibrated device. Windows uses Stream Engine 4; the selected Linux
SDK uses Stream Engine 3. SDK files and vendor runtime DLLs are not bundled.
Without a connected tracker, mouse dwell remains available.

## Documentation

- [Using Gaze Grid](docs/usage.md): input, composition, prediction, gaze, settings, and recovery.
- [Keyboard layouts and editor](docs/keyboards.md): XML, canvas layouts, dwell groups, and visual editing.
- [Virtual game controller](docs/game-controller.md): setup and action reference.
- [Development and diagnostics](docs/development.md): builds, isolated tests, and experimental Plasma input method.
- [Windows](docs/windows.md): installation, signing, startup, and secure-desktop integration.
- [Roadmap](docs/roadmap.md): scope, remaining work, and acceptance gates.
- [Historical acceptance](docs/archive/host-acceptance.md) and [Plasma research](docs/archive/plasma-system-keyboard-research.md).
- [Security audit](docs/security-audit-2026-11-08.md) and [release notes](docs/releases/).

Windows x64 and Linux/KDE Wayland have implementations and automated tests.
Hardware and assistive usability checks remain distinct from those tests;
see the roadmap for outstanding acceptance. macOS and Windows ARM64 are unsupported.

## Licensing and identity

See [LICENSE.txt](LICENSE.txt), [resource attribution](resources/ATTRIBUTION.md),
[prediction model attribution](resources/prediction/README.md),
[multilingual models](resources/prediction/MULTILINGUAL.md), and
[protocol provenance](protocols/README.md).

The display name is **Gaze Grid**. Build targets, executable names, settings paths,
and registry identifiers retain `gaze-grid`; the desktop ID is
`org.gaze_grid.gaze_grid`, and runtime overrides use the `GAZE_GRID_` prefix.
Legacy OptiKey settings and plugins are not imported.
