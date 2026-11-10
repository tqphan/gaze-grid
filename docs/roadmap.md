# Roadmap and acceptance gates

Consolidated on 2026-11-09 from the former modernization plan and feature inventory.
This records remaining work and retained decisions, not a new hardware validation.
Current usage is in [the user guide](usage.md); historical results are in
[host acceptance](archive/host-acceptance.md).

## Scope and retained decisions


- Preserve the required OptiKey Pro behavior and keyboard pack as the reference. Keep rewrite work in this project. Linux/KDE Wayland and Windows x64 have implementations; macOS remains future work.
- Use Qt 6 Quick/QML, C++20 and CMake. Keep platform APIs behind input/output interfaces and selection logic independent of SDKs.
- Support direct loading of the required XML keyboard pack, including game and mouse actions. Preserve source layouts and make optional visual overrides explicit. Full original-app behavioral parity remains a release gate.
- Use explicit, visible platform shortcut mappings with a literal option; preserve physical game-control semantics.
- Linux uses the selected Tobii Stream Engine 3.0.4.6031 SDK with matching headers. Windows uses Stream Engine 4 headers and a compatible vendor-installed runtime. Do not bundle vendor SDK files or runtime DLLs.
- Keep prediction offline and learning optional and resettable. Do not load legacy plugins or migrate legacy OptiKey settings. A new plugin system and automatic updater are outside the agreed scope.
- Deliver locally first; produce an Arch package after desktop-control and live Tobii acceptance. Retain GPL licensing and resource attribution.
- Prefer RemoteDesktop portal/libei. A narrowly scoped, unprivileged-client system helper is an accepted fallback only after demonstrating a specific blocker. Any helper design and installation requirements must be reviewable; this is not authorization to install a privileged service.


## Implemented baseline

Windows keyboard/pointer output, speech, tracker integration, signed MSI packaging,
startup and secure-desktop integration have implementation and automated coverage.
Linux has portal/libei output and an experimental Plasma input-method backend.
Offline multilingual prediction, supported-field caret context, local composition,
conversation profiles, and Linux/Windows CI are implemented. These are no longer
listed as future implementation tasks; real-device acceptance remains separate.

## Remaining work and validation

| Area | Remaining work or acceptance |
| --- | --- |
| Selection | Advanced multi-key typing, additional switch/controller trigger adapters, and assistive usability of dwell/repeat modes. Virtual gamepad output does not provide physical-controller trigger input. |
| Keyboard parity | Compare required XML geometry, symbols, navigation, state persistence, disabled/held visuals, macros, and mapped/literal shortcuts with the original app. Inventory CommuniKate behavior and remaining layouts/translations. |
| Text input and prediction | Validate real editors, browsers, GTK and XWayland, dead keys, alternate layouts, IME candidates, accessibility-provider coverage, external edits, and best-effort context limitations. Extend script-specific gaze mappings where needed. |
| Pointer and magnifier | Validate live gaze targeting, drag/scroll parameters, modifier interactions, progressive magnification, and minimized recovery. |
| Displays | Mixed-DPI, hotplug, rotation, coordinate remapping, multi-display targeting, and explicit tracker/display calibration mapping. |
| Speech and profiles | Live playback/cancellation, stalled speech-service recovery, assistive settings operation, conversation profiles, and preference recovery. |
| Tobii | Calibration availability/accuracy, live dwell typing, unplug/replug, suspend/resume, and sustained use. Historical Linux streaming evidence does not establish current Windows hardware acceptance. |
| Desktop lifecycle | Permission restoration/revocation, session recreation, focus preservation, suspend/resume, and recovery using the intended assistive input. |
| Plasma input method | Real-session discovery/startup, Qt/GTK/browser/XWayland compatibility, focus/recovery, offset/minimized native layouts, live gaze and DPI behavior. Select as the default only after validation, retaining rollback instructions. |
| Windows | Signed installation, input in elevated apps, SAPI and virtual-controller hardware acceptance, real layout/IME tests, and sign-in/lock/UAC transitions with Tobii available in the secure session. See [Windows acceptance notes](windows.md). |
| Security | Validate personal-file permissions across users; review live signing/environment protections and run privileged installer trust lifecycle cases. See the [dated audit](security-audit-2026-11-08.md) for findings and verification limits. |
| Delivery | Dependency/resource license inventory, reproducible release validation, Arch packaging after acceptance, other distributions/compositors, and representative legacy regression fixtures. |

## Release acceptance

- Compare the required pack with the reference application in real target apps.
- Verify gaze calibration, dwell typing, pointer targeting, magnification, and recovery.
- Verify permission denial/revocation, reconnect, cancellation, and owned-input cleanup.
- Exercise speech, settings, profiles, and preference recovery with the intended input.
- Exercise mixed-DPI/rotated displays, hotplug, and tracker/display mapping.
- Complete a 60-minute session without crashes, duplicate selections, or stuck input.
  Measure selection-to-output latency separately from dwell; the original target
  was p95 below 50 ms on the reference host, not an established cross-device guarantee.

## Future platform work

After Linux acceptance, package for Arch and validate another distribution and
GNOME Wayland separately. Evaluate Flatpak after portal, input, and surface behavior
is proven inside its sandbox. An X11 backend remains conditional on the support matrix.

Investigate Plasma Login and screen-locker integration separately; user-session
input-method registration does not establish login-screen support.
See [archived Plasma research](archive/plasma-system-keyboard-research.md).

macOS requires a non-activating panel bridge, native output, speech, display
transforms, Accessibility permission recovery, hardware checks, and signed/notarized
artifacts. Begin with Apple Silicon and determine the minimum OS from the selected
Qt release. Validate Spaces, fullscreen, scaling, layouts, and permission revocation.
Report tracker support per device and OS. Windows ARM64 is not currently supported.

## Acceptance rules

Keep deterministic replay and fake-backend tests separate from real desktop/device
evidence. Test owned-key/button cleanup and cancellation through failures; never
replay stale actions after reconnecting. Do not claim support based only on
compilation, rendering, or handler availability. Keep typed text and gaze coordinates
out of default diagnostics. Implementation alone does not close a live acceptance gate.
