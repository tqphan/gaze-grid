# Windows build, integration and MSI releases

Supported CI platforms are Linux x64 (Arch Linux userspace) and Windows x64
(MSVC 2022, Qt 6.12 LTS). macOS and Windows ARM64 are not currently supported.
The root CMake project now builds the full application on Windows. The earlier
`platform/windows` project remains available for isolated input-adapter tests.

## Integrated application

Windows uses SendInput for keyboard/pointer output, primary-screen desktop
capture for magnification, and SAPI speech on the speech worker thread. Existing
pause, cancellation, tracking-loss and shutdown paths release owned input.
Primary-display gaze coordinates are translated to physical virtual-desktop
coordinates before SendInput; capture uses that same primary display. Qt's
non-focus-taking, topmost window flags apply to the keyboard and target overlay.
Settings uses Qt Quick Controls without KDE QML modules. Keyboard layouts,
editor, prediction and dwell share the Linux application implementation.
Windows executes the XML pack's original shortcuts and bypasses Linux mappings.
Desktop input connects automatically during normal startup.

The installed application finds its keyboard pack beside the executable, rather
than relying on the source checkout or working directory. Program Files is not
writable by standard users: use the editor's Save As to export modified layouts
rather than expecting in-place edits to the bundled pack to succeed.

Windows virtual-gamepad output uses ViGEmBus and presents an Xbox 360 controller.
Install the [ViGEmBus driver](https://github.com/nefarius/ViGEmBus/releases) before
using gamepad output. gaze-grid links ViGEmClient statically, creates a target on
first output, neutralizes it on reset/pause, and removes it on shutdown or output
failure. Missing-driver errors include setup guidance; a later action retries.
Standard buttons, D-pad/hat axes, two sticks, analog triggers and trigger buttons
are supported. C/Z, Button1–32, Throttle, Rudder and Wheel have no Xbox 360
mapping and are rejected. Rumble feedback is not implemented.
The shared action logic and Xbox report/lifecycle tests use injected output, so
CI never needs to create a virtual device or install a kernel driver. Physical input follows the target application's supported system keyboard layout. Mixed-DPI displays,
focus retention, elevated application input, real SAPI playback and eye-tracker
hardware require Windows acceptance testing. UIAccess does not grant control of
the secure desktop. Native Windows build and installer results must come from
Windows CI; running the Linux suite does not validate those paths.

## Development build

Install Visual Studio 2022 C++ tools, Windows SDK, CMake 3.24+, and Qt 6.12 LTS MSVC
x64 (including SVG). Put Qt's `bin` on PATH and set `CMAKE_PREFIX_PATH` to its
installation. CMake fetches ViGEmClient at pinned commit
`b66d02d57e32cc8595369c53418b843e958649b4`; for offline builds, pass
`-DFETCHCONTENT_SOURCE_DIR_VIGEMCLIENT=C:/SDK/ViGEmClient` with that checkout.
The MSI includes the statically linked client and its license; the bus driver
remains a separate prerequisite and retains its upstream signature.
In a Visual Studio developer PowerShell:

```powershell
cmake -S . -B build/windows -A x64 -DGAZE_GRID_WITH_TOBII=OFF -DGAZE_GRID_UIACCESS=OFF
cmake --build build/windows --config Release --parallel
ctest --test-dir build/windows -C Release --output-on-failure
./build/windows/Release/gaze-grid.exe
```

UIAccess defaults to ON for Windows. Turn it off for unsigned development builds
launched outside Program Files. Tests use an ordinary executable, then the
release script relinks with UIAccess before signing. The script checks the
embedded manifest, including `asInvoker` and `uiAccess=true`.

`GAZE_GRID_WITH_TOBII=OFF` builds the full UI with mouse dwell and reports that
tracker support is unavailable. CI uses this option because proprietary SDK
headers and binaries are not stored in the repository. For eye-tracker builds,
set `GAZE_GRID_WITH_TOBII=ON`, `TOBII_SDK` to a Windows Stream Engine 4 SDK
with `include/tobii`. The build needs SDK headers, but does not bundle a runtime DLL. The existing adapter uses
the Stream Engine 4 API on Windows; a different SDK API requires adapting the loader.
Windows users must install the [Tobii Experience driver package and app](https://help.tobii.com/hc/en-us/articles/360010104134-How-to-install-Tobii-Experience),
complete Tobii setup/calibration, and restart gaze-grid. This vendor installation
supplies the Stream Engine runtime. Install the device drivers and driver software
before the Experience app; the Microsoft Store app alone is not the full runtime
installation. gaze-grid searches the `Tobii` and
`Tobii Gaming` installation trees under Program Files (including the x86 tree,
since it may contain x64 components), then loads a compatible x64 DLL by absolute
path. It does not search the working directory or PATH or follow directory
links outside those trees. If your package installs elsewhere, report the
package/version and installation path so discovery can be extended.

The Windows adapter uses the Stream Engine **4** ABI; a DLL with a different
major version is rejected before device/API calls. Installing a newer runtime
alone does not make that ABI compatible. Missing or incompatible runtimes report
setup guidance; mouse dwell remains available. The MSI explains this prerequisite
and whether the particular gaze-grid build includes tracker support.

### Public integration reference

The Windows implementation follows the public
[App-TobiiStreamEngine C++ example](https://github.com/labstreaminglayer/App-TobiiStreamEngine/blob/master/mainwindow.cpp),
particularly `device_connect()` and `recording_thread_function()`. This is an
integration example, not the vendor's API specification. Device creation passes
`TOBII_FIELD_OF_USE_INTERACTIVE` as the third argument, before the device output
pointer. API creation, device enumeration, gaze subscription, callback processing,
and teardown use the same basic lifecycle.

Gaze Grid retains interruptible polling and input-release behavior. Invalid samples
pause gaze selection; sample timestamps and gaps alone do not invalidate gaze. The Linux build continues to use its selected Stream Engine 3
SDK and three-argument device creation. Windows still needs matching SDK headers
at build time and a compatible x64 runtime at execution time; the public example
does not replace those dependencies or establish hardware compatibility.

## Persistent gaze-grid signing identity

Create the identity once on a Windows signing machine:

```powershell
./platform/windows/New-gaze-gridCertificate.ps1
```

The script prompts for an output directory (press Enter to use
`gaze-grid-Signing` under your Windows user profile) and a password, with input
hidden. Save that password for the GitHub signing secret. You can still pass
`-OutputDirectory` and a SecureString `-Password` for automated use.
After creation, the script prints the GitHub environment/secret setup, release
workflow, artifact download, and target-machine certificate trust instructions.
Interactive runs also offer to copy the base64-encoded PFX to the clipboard;
fully parameterized CI calls do not pause for this prompt. The script never
prints the password or encoded PFX.

This creates a self-signed RSA/SHA-256 code-signing certificate with subject and
friendly name gaze-grid, exports a password-protected PFX and a public CER, and
prints its thumbprint. Retain that identity for subsequent releases. Private
keys must stay outside source control and release artifacts; `.gitignore`
excludes PFX/P12 files. The script does not automatically trust the certificate.

The MSI asks whether to trust the public gaze-grid certificate. **Accept** is
selected by default. **Decline** installs an unsigned executable with `uiAccess=false`
and warns that the app cannot send input to higher-privilege windows or stay on
top of all windows. Ordinary desktop input remains available. **Accept** installs
the public certificate into **Local Computer / Trusted Root Certification
Authorities** and installs the signed `uiAccess=true` executable. Both variants
install under `C:\Program Files\gaze-grid`. The MSI itself is always signed.

Verify the certificate thumbprint before accepting. Trust applies to all users
and other software signed by that identity; it does not establish public trust
or SmartScreen reputation. Only the public certificate is embedded in the MSI.
Uninstall removes trust added by this installer. Trust already present before
installation is preserved, including trust left by older releases. To revoke
pre-existing trust, an administrator can use `certlm.msc`, open Trusted Root
Certification Authorities / Certificates, verify the thumbprint, and remove
that gaze-grid certificate. Other software using the same signing identity may
also depend on that trust.

Unattended installation defaults to the signed UIAccess app and certificate
trust. To decline explicitly:

```powershell
msiexec /i gaze-grid.msi /qn GAZE_GRID_TRUST_CERTIFICATE=0
```

Repairs retain the installed mode. Uninstall and reinstall to change modes;
major upgrades ask again and unattended upgrades default to Accept. Declining
never removes existing certificate trust, but still installs the unsigned app.

Microsoft documents the signature and secure-location requirements in
[UIAccess policy](https://learn.microsoft.com/en-us/windows/security/threat-protection/security-policy-settings/user-account-control-allow-uiaccess-applications-to-prompt-for-elevation-without-using-the-secure-desktop).

## Completion options

**Start with Windows** is selected by default during setup. Uncheck it, or pass
`GAZE_GRID_STARTUP=0` for a silent installation, to disable desktop startup.

After a successful interactive installation, the completion screen offers
**Launch Gaze Grid after setup**, selected by default. Uncheck it to finish
without opening the app. Clicking **Finish** launches the installed executable
through WiX's `WixShellExec` action, after files and certificate trust have
committed. This uses Windows `ShellExecuteEx`, which supports UIAccess, in the
installer's user interface process without requesting `runas`.
Quiet installs, repairs, uninstalls, failures, and cancellations do not launch
the app. This completion choice is separate from Windows sign-in startup below.

## Keyboard before Windows sign-in

The trusted installation registers gaze-grid with Windows Ease of Access. Setup
selects **Start Gaze Grid before Windows sign-in** by default, including silent
installations. Uncheck it in setup or pass `GAZE_GRID_SIGN_IN=0` to disable it.
Declining certificate trust automatically disables sign-in startup for the
unsigned installation. Repairs preserve the selected choice, including an
earlier decision to disable startup.
This uses Windows' assistive-technology startup mechanism, rather than a user
Startup-folder shortcut. It does not replace any Windows accessibility executable
or disable the secure desktop.

To copy your current preferences after installation, open **Settings → Keyboard
→ Windows sign-in**, then clear and select **Use this keyboard at sign-in**.
Alternatively, click **Sync current settings** in the same section to refresh
the login and secure desktop preferences without changing sign-in startup.
This also works when sign-in startup is disabled, for UAC use while gaze-grid is running.
Accept the administrator prompt to copy the selected
installed layout, keyboard appearance, dwell timing, and gaze preferences to a
protected machine-wide configuration. Use **Sync current settings** again after changing
preferences. Clearing **Use this keyboard at sign-in** removes gaze-grid from startup
without changing other accessibility tools. Repair preserves the selected mode;
uninstall removes gaze-grid's registration and startup entry.

Windows launches the same `gaze-grid.exe` with `--accessibility` on either desktop.
gaze-grid acquires a session-local lock for the window station and desktop before
loading its interface or starting eye tracking. Repeated launches, including
broker launches, exit without sending accessibility notifications. gaze-grid
detects the Winlogon desktop and selects the restricted sign-in interface there.
It uses the existing keyboard rendering, navigation, key output, and dwell logic.
Prediction stays off even after switching layouts. Conversation layouts allow
editing the local scratchpad in memory; keys in desktop-control layouts are not
mirrored into it. Typed text is not learned. Personal profile files are not read or written,
and Settings and the editor are unavailable in this mode. The magnifier can be
toggled using the keyboard; its zoom and dwell preferences are copied when
sign-in startup is configured. Magnifier toggles on the secure desktop are not
saved. Actual secure-desktop capture still requires Windows acceptance testing. The
registration uses `TerminateOnDesktopSwitch=0`: the normal instance stays alive
and suspends input and eye tracking while its desktop is inactive. It resumes
when that desktop becomes active again. A secure instance exits when its desktop
loses input. The eye-tracker worker is stopped and joined during suspension;
queued samples from the previous connection are discarded on resume.

Normal desktop preferences and prediction remain unchanged. Until preferences
are copied from Settings, the sign-in keyboard uses the default alphabet layout
with gaze enabled. An explicitly copied gaze-off preference is preserved. Failed
tracker startup retries after 1, 2, 4 seconds and so on, capped at 30 seconds,
while gaze is enabled and the desktop is active. A status message on the sign-in
keyboard shows connection errors and retry timing; it disappears after connecting.
Mouse dwell remains available when the tracker is disconnected. The Tobii runtime,
services, device, and calibration must be usable by the Windows sign-in session.

Registration and startup follow [Microsoft's assistive-technology
registration](https://learn.microsoft.com/en-us/windows/win32/winauto/ease-of-access---assistive-technology-registration).
The MSI modifies only gaze-grid's item in the machine accessibility startup list,
with rollback support, preserving existing entries such as Narrator and OSK.
The administrator helper accepts only validated layout names and keyboard/gaze
preferences; it cannot load arbitrary paths or copy typed text.

The same trusted registration also lets Windows launch the restricted keyboard
on UAC prompts while gaze-grid is running. Only the owning normal-desktop instance
notifies Ease of Access when it starts or exits, using the documented
`AccessibilityTemp` signal and Windows+U notification. Secure instances never
send those notifications. The elevation helper runs on a worker thread so the
UI can keep monitoring desktop changes during its own administrator prompt.
The installer leaves
`SecureDesktopAccommodation` unset so Windows uses gaze-grid itself. That value
is an optional **string naming an alternate registered accessibility app**, not
a numeric enable flag. Repairing an updated MSI removes the old DWORD override
and replaces the old `--sign-in` startup arguments. Repair also switches the
registration from the incompatible managed-job mode to the direct-launch mode.
Install the trusted signed build in
Program Files, restart gaze-grid, then check a UAC prompt on the secure desktop.
Enabling startup before sign-in is a separate choice and is not required for
UAC support while gaze-grid is already running.

Automated checks cover the existing UI in sign-in mode, prediction/learning and
text-retention suppression, validated preferences, and accessibility-list
preservation, tracker startup recovery, desktop suspension, and stale queued
samples. Native subprocess tests cover duplicate rejection, process death, and
independent desktop locks. Installer CI exercises a real owning keyboard and
repeated broker-style launches in addition to default startup, opt-out, repair,
and uninstall.
Actual sign-in, lock/unlock, and UAC desktop transitions require acceptance on a signed Windows
installation. Test PIN/password entry, top/bottom placement, gaze loss, and
return to the user's normal keyboard. Tobii must be available to the Windows
sign-in session; desktop tracking alone does not establish this. Hardware
sign-in acceptance remains pending.

## Build a signed MSI

Install WiX 3 and put its `bin` directory on PATH. Import the persistent PFX into
Current User / Personal on the signing machine; ensure its public certificate
is trusted there so Authenticode verification can succeed. Then run:

```powershell
./platform/windows/Build-Release.ps1 -CertificateThumbprint YOUR_THUMBPRINT
# Tracker-enabled package:
./platform/windows/Build-Release.ps1 -CertificateThumbprint YOUR_THUMBPRINT `
  -TobiiSdk C:/SDK/StreamEngine
```

The script builds/tests, relinks the UIAccess executable, checks its embedded
manifest, signs and verifies the executable, deploys Qt/QML, creates the MSI via
CPack/WiX, then signs and verifies the MSI with the same gaze-grid certificate.
It exports only the public certificate alongside the MSI. Packages are under
`build/windows`. Signatures use SHA-256; no timestamp service is configured, so
renew and re-sign releases before the signing certificate expires.

## CI

`.github/workflows/build.yml` builds and tests Linux and Windows on pushes and
pull requests. Windows additionally packages a signed MSI with an ephemeral
**test-only** gaze-grid identity, verifies installation and installed application
startup from outside the build tree, confirms Windows actually granted the
process a UIAccess token via `--diagnose`, and checks the default signed mode,
repair, and accepting or declining with a previously trusted certificate.
Uninstall checks verify removal of installer-owned trust and preservation of
pre-existing trust. The installed smoke test
removes Qt development search paths and launches through Windows ShellExecute,
matching a normal Start menu launch. `--diagnose --diagnostic-output PATH` writes
the JSON report to a file because UIAccess launches cannot redirect stdout. CI artifacts include MSI and public CER,
never the PFX. Do not distribute these changing CI identities as releases.

`.github/workflows/windows-release.yml` runs when a `v*` tag is pushed or from
**Actions → Signed Windows MSI → Run workflow**. It uses the `windows-release`
environment and these secrets:

- `GAZEGRID_SIGNING_PFX_BASE64`: base64-encoded existing persistent GazeGrid PFX.
- `GAZEGRID_SIGNING_PFX_PASSWORD`: its export password.
- `GAZEGRID_TOBII_HEADERS_BASE64`: a base64 ZIP containing only
  `include/tobii/tobii.h` and `include/tobii/tobii_streams.h` from Stream Engine
  4.23.0.3. The archive SHA-256 and exact file list are pinned in
  `platform/windows/tobii-sdk.json`; `Restore-TobiiSdk.ps1` verifies both before
  extracting. SDK headers stay out of the repository and release assets.

The tag must exactly match `v` plus the version in `CMakeLists.txt`, for example
`v0.1.0`. For a new release, update that version, commit and push the change, then
push the matching tag. Alternatively, manually run the workflow on the desired
commit with **Create a draft GitHub release** enabled (the default); it selects
the project version automatically. Uncheck that option for an artifact-only build.

After build, tests, signing and installed UIAccess verification succeed, the
workflow prepares `gaze-grid-VERSION-Windows-x64.msi`, `gaze-grid.cer`, SHA-256
checksums, and release notes. A separate job with repository write permission
attaches the MSI, CER and checksum file to a **draft GitHub release**. Review it
under the repository's Releases page and publish when ready. The release notes
include the source commit, signing certificate thumbprint and runtime prerequisites.
An existing version tag must point to the same source commit. Reruns may refresh
a draft; published release assets are never overwritten.

The release workflow requires the signing and SDK-header secrets; it does not
silently produce a mouse-only release. Published Windows builds include Tobii
support and load the compatible Stream Engine 4 DLL from the user's Tobii
installation. No vendor DLL is bundled. Packaging checks both the deployment
directory and the MSI File table for Tobii DLLs; installed-app diagnostics verify
that tracker support was compiled in, even on CI machines without Tobii hardware.

### Qt MinGW quick start

With Qt 6.12 LTS MinGW x64, its matching compiler, CMake and Ninja installed under C:\Qt, run `.\build.ps1 -Run` or double-click `run.cmd`. The script configures an unsigned development build, runs tests, and deploys Qt beside the executable. Use `-QtDirectory` to select a kit, `-QtRoot` for a different Qt installation root, and `-TobiiSdk` for eye-tracker SDK headers. Signed UIAccess releases still require MSVC.

## Start with Windows

Select **Start with Windows** in setup or under **Settings > System > Startup**
to launch gaze-grid after signing in. Setup selects it by default; silent
installations can opt out with `GAZE_GRID_STARTUP=0`. Repair retains the setup
choice and restores a missing launch entry when startup was selected.
This registers the app for the
current user in Windows **Settings > Apps > Startup** and Task Manager. Changes
apply immediately without saving preferences. Windows startup controls can also
disable it. Startup before sign-in remains a separate option. Uninstall removes
the startup entry for the user running uninstall; other users can turn theirs off
in Settings before uninstalling.
