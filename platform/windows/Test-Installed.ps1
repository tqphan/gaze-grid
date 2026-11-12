# Run only on an isolated Windows test machine: temporarily removes this test root.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Msi, [Parameter(Mandatory)][string]$CertificateThumbprint, [switch]$ExpectTobii, [switch]$ExpectLocalIme, [string]$UpgradeFromMsi)
$ErrorActionPreference = 'Stop'
$path = (Resolve-Path $Msi).Path
& "$PSScriptRoot/Assert-NoTobiiRuntime.ps1" -Msi $path
$installed = [IO.Path]::GetFullPath("$env:ProgramFiles/gaze-grid/gaze-grid.exe")
function Run-Msi([string]$Operation, [string[]]$Properties = @(), [string]$Package = $path) {
    $process = Start-Process msiexec.exe -ArgumentList (@($Operation, "`"$Package`"", '/qn', '/norestart', '/l*v', "`"$env:RUNNER_TEMP/gaze-grid-msi-$mode-$($Operation.TrimStart('/')).log`"") + $Properties) -Wait -PassThru
    if ($process.ExitCode -notin @(0, 3010)) { throw "MSI failed: $($process.ExitCode)" }
}

# Unlike a fresh install or repair, an upgrade costs components while the old
# Run entry still exists, then removes the old product before writing new keys.
function Test-StartupUpgrade {
    $previous = (Resolve-Path -LiteralPath $UpgradeFromMsi).Path
    $baseline = (Get-Content "$PSScriptRoot/release-trust.json" -Raw | ConvertFrom-Json).upgradeBaseline
    if ((Get-FileHash -LiteralPath $previous -Algorithm SHA256).Hash -ne $baseline.sha256) { throw 'Upgrade baseline checksum mismatch.' }
    Import-Certificate -FilePath $testCertificate -CertStoreLocation Cert:\LocalMachine\Root | Out-Null
    try {
        $signature = Get-AuthenticodeSignature -LiteralPath $previous
        if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $baseline.thumbprint) { throw 'Upgrade baseline signature mismatch.' }
    } finally { Remove-Item "Cert:\LocalMachine\Root\$CertificateThumbprint" -Force }
    $mode = 'startup-upgrade-baseline'
    Run-Msi '/i' @('GAZE_GRID_STARTUP=1', 'GAZE_GRID_SIGN_IN=0') $previous
    try {
        Test-DesktopStartup $true
        $approvalKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run'
        New-Item -Path $approvalKey -Force | Out-Null
        # Windows-disabled startup must remain disabled while its Run entry
        # survives the upgrade. The installer must not reset user approval.
        [byte[]]$disabled = @(3,0,0,0,0,0,0,0,0,0,0,0)
        New-ItemProperty -LiteralPath $approvalKey -Name 'gaze-grid' -Value $disabled -PropertyType Binary -Force | Out-Null
        $mode = 'startup-upgrade'
        Run-Msi '/i' @('GAZE_GRID_STARTUP=1', 'GAZE_GRID_SIGN_IN=0')
        Test-DesktopStartup $true
        $approval = (Get-ItemProperty -LiteralPath $approvalKey -Name 'gaze-grid').'gaze-grid'
        if ([BitConverter]::ToString($approval) -ne [BitConverter]::ToString($disabled)) { throw 'Upgrade changed Windows startup approval.' }
    } finally {
        # The old product is gone after a successful major upgrade. On failure
        # remove whichever product remains using its own source package.
        $mode = 'startup-upgrade-cleanup'
        $installer = New-Object -ComObject WindowsInstaller.Installer
        foreach ($package in @($path, $previous)) {
            $database = $installer.OpenDatabase($package, 0)
            $view = $database.OpenView('SELECT `Value` FROM `Property` WHERE `Property`=''ProductCode''')
            $view.Execute(); $record = $view.Fetch(); $code = $record.StringData(1)
            $view.Close()
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($record)
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($view)
            [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($database)
            if ($installer.ProductState($code) -eq 5) { Run-Msi '/x' @() $package }
        }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($installer)
    }
    Test-DesktopStartup $false
    Write-Host 'Startup registration survived a major upgrade without changing Windows startup approval.'
}
function Test-KeyboardInstanceLifecycle {
    $sessionId = (Get-Process -Id $PID).SessionId
    function Get-TestKeyboards {
        @(Get-CimInstance Win32_Process -Filter "Name='gaze-grid.exe'" | Where-Object {
            $_.SessionId -eq $sessionId -and $_.ExecutablePath -eq $installed
        })
    }
    if (@(Get-TestKeyboards).Count) { throw 'Unexpected keyboard process before the instance lifecycle test.' }
    $passed = $false
    try {
        # Exercise the actual installed startup path; --smoke/--diagnose bypass
        # both the singleton and accessibility notifications.
        $owner = Start-Process $installed -Verb Open -WindowStyle Hidden -ArgumentList '--accessibility' -PassThru
        $null = $owner.Handle
        if ($owner.WaitForExit(1000)) { throw 'The owning keyboard exited during startup.' }
        foreach ($attempt in 1..3) {
            $duplicate = Start-Process $installed -Verb Open -WindowStyle Hidden -ArgumentList '--accessibility' -PassThru
            $null = $duplicate.Handle
            if (!$duplicate.WaitForExit(5000)) { throw 'A broker-style duplicate remained running.' }
            if ($duplicate.ExitCode -ne 0) { throw 'A duplicate launch did not exit successfully.' }
        }
        $keyboards = @(Get-TestKeyboards)
        if ($keyboards.Count -ne 1 -or $keyboards[0].ProcessId -ne $owner.Id) { throw 'Repeated launches created additional keyboard instances.' }
        $passed = $true
    } finally {
        if (!$passed) {
            # This fixture runs on an isolated machine. Break a regressed broker
            # loop before cleanup so a failed test cannot exhaust the runner.
            Remove-Item -LiteralPath 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Accessibility\ATs\gaze-grid_gaze-grid_v1' -Recurse -Force -ErrorAction SilentlyContinue
        }
        Get-TestKeyboards | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    }
}
$testCertificate = "$env:RUNNER_TEMP/gaze-grid-preexisting-test.cer"
Export-Certificate -Cert "Cert:\LocalMachine\Root\$CertificateThumbprint" -FilePath $testCertificate | Out-Null
Remove-Item "Cert:\LocalMachine\Root\$CertificateThumbprint" -Force
$accessibility = 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Accessibility'
$atName = 'gaze-grid_gaze-grid_v1'
$desktopRun = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
function Test-DesktopStartup([bool]$Expected) {
    $entry = (Get-ItemProperty -LiteralPath $desktopRun -Name 'gaze-grid' -ErrorAction SilentlyContinue).'gaze-grid'
    if ($Expected) {
        if ($entry -ne ('"' + $installed + '"')) { throw 'Desktop startup entry is missing or points to the wrong executable.' }
    } elseif ($entry) { throw 'Desktop startup was enabled despite opting out.' }
}
$originalStartup = @(((Get-ItemProperty $accessibility -Name Configuration -ErrorAction SilentlyContinue).Configuration -split ',') | Where-Object { $_ -and $_.Trim() -ne $atName })
foreach ($mode in @('decline', 'default', 'accept', 'sign-in', 'sign-in-off', 'decline-trusted', 'accept-trusted', 'startup-off')) {
Write-Host "Checking installation mode: $mode"
$preexistingTrust = $mode -in @('decline-trusted', 'accept-trusted')
if ($preexistingTrust) { Import-Certificate -FilePath $testCertificate -CertStoreLocation Cert:\LocalMachine\Root | Out-Null }
$expectedUIAccess = $mode -in @('default', 'accept', 'accept-trusted', 'sign-in', 'sign-in-off')
$expectedSignIn = $expectedUIAccess -and $mode -ne 'sign-in-off'
[string[]]$properties = if ($mode -eq 'default') { @() } elseif ($expectedUIAccess) { @('GAZE_GRID_TRUST_CERTIFICATE=1') } else { @('GAZE_GRID_TRUST_CERTIFICATE=0') }
if ($mode -eq 'sign-in') { $properties += 'GAZE_GRID_SIGN_IN=1' }
if ($mode -eq 'sign-in-off') { $properties += 'GAZE_GRID_SIGN_IN=0' }
if ($mode -eq 'startup-off') { $properties += 'GAZE_GRID_STARTUP=0' }
Run-Msi '/i' $properties
try {
    Test-DesktopStartup ($mode -ne 'startup-off')
    if ($mode -eq 'default') {
        # Leave the old installer marker in place: it must not hide a missing Run entry.
        Remove-ItemProperty -LiteralPath $desktopRun -Name 'gaze-grid'
    }
    $rootTrusted = Test-Path "Cert:\LocalMachine\Root\$CertificateThumbprint"
    if ($rootTrusted -ne ($expectedUIAccess -or $preexistingTrust)) { throw "Unexpected certificate trust in $mode installation." }
    $startup = @(((Get-ItemProperty $accessibility -Name Configuration -ErrorAction SilentlyContinue).Configuration -split ',') | Where-Object { $_ })
    if (($atName -in $startup) -ne $expectedSignIn) { throw 'Unexpected sign-in startup selection after installation.' }
    # A repair must also remove the legacy DWORD accommodation and the
    # sign-in-only arguments, which prevented normal-desktop AT restarts.
    if ($expectedUIAccess) {
        New-ItemProperty "$accessibility\ATs\$atName" -Name SecureDesktopAccommodation -Value 1 -PropertyType DWord -Force | Out-Null
        New-ItemProperty "$accessibility\ATs\$atName" -Name StartParams -Value '--sign-in' -PropertyType String -Force | Out-Null
        New-ItemProperty "$accessibility\ATs\$atName" -Name TerminateOnDesktopSwitch -Value 1 -PropertyType DWord -Force | Out-Null
    }
    # A repair with no consent property must preserve the original mode.
    Run-Msi '/fomus'
    if ((Test-Path "Cert:\LocalMachine\Root\$CertificateThumbprint") -ne ($expectedUIAccess -or $preexistingTrust)) { throw 'Repair changed certificate trust.' }
    Test-DesktopStartup ($mode -ne 'startup-off')
    $startup = @(((Get-ItemProperty $accessibility -Name Configuration -ErrorAction SilentlyContinue).Configuration -split ',') | Where-Object { $_ })
    if (($atName -in $startup) -ne $expectedSignIn) { throw 'Unexpected sign-in startup selection after repair.' }
    if (@(Compare-Object $originalStartup @($startup | Where-Object { $_.Trim() -ne $atName })).Count) { throw 'Other accessibility startup entries changed.' }
    if ($mode -eq 'sign-in-off') {
        # The UI checkbox saves an empty string when unchecked; older installs
        # also used that value. It must not inherit the new enabled default.
        Set-ItemProperty 'HKLM:\SOFTWARE\gaze-grid\Installer' -Name SignIn -Value ''
        Run-Msi '/fomus'
        $startup = @(((Get-ItemProperty $accessibility -Name Configuration -ErrorAction SilentlyContinue).Configuration -split ',') | Where-Object { $_ })
        if ($atName -in $startup) { throw 'Repair enabled startup after an unchecked sign-in choice.' }
        if (@(Compare-Object $originalStartup $startup).Count) { throw 'Repair changed another accessibility startup entry.' }
    }
    $registration = Get-ItemProperty "$accessibility\ATs\$atName" -ErrorAction SilentlyContinue
    if ($expectedUIAccess -and (!$registration -or [IO.Path]::GetFullPath($registration.StartExe) -ne [IO.Path]::GetFullPath($installed))) { throw 'Trusted sign-in registration is missing or incorrect.' }
    if (!$expectedUIAccess -and $registration) { throw 'Unsigned installation must not register secure-desktop startup.' }
    if ($expectedUIAccess -and ($registration.PSObject.Properties.Name -contains 'SecureDesktopAccommodation')) { throw 'Same-app secure desktop support must use the default accommodation, without an override.' }
    if ($expectedUIAccess -and $registration.StartParams -ne '--accessibility') { throw 'Windows must use the desktop-independent accessibility launch marker.' }
    if ($expectedUIAccess -and $registration.TerminateOnDesktopSwitch -ne 0) { throw 'Direct accessibility launches require the unmanaged desktop lifecycle.' }

    $signature = Get-AuthenticodeSignature $installed
    if ($expectedUIAccess -and ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Thumbprint -ne $CertificateThumbprint)) {
        throw 'Installed application signature did not match gaze-grid.'
    }
    if (!$expectedUIAccess -and $signature.Status -ne 'NotSigned') { throw 'Declining must install the unsigned executable.' }
    if ($mode -eq 'sign-in') {
        # The isolated CI runner is already elevated. Exercise the same helper
        # that Settings launches with an administrator prompt on a user's PC.
        $preferences = [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes('{"startLayout":"main-14x1-top","keyboardScale":120,"gazeEnabled":false}'))
        $helper = Start-Process $installed -Verb Open -ArgumentList @('--configure-sign-in', 'enable', $preferences) -PassThru
        $null = $helper.Handle
        if (!$helper.WaitForExit(30000)) { $helper.Kill(); throw 'Sign-in configuration helper timed out.' }
        if ($helper.ExitCode -ne 0) { throw 'Could not copy the selected keyboard to sign-in preferences.' }
        $shared = Get-ItemProperty 'HKLM:\SOFTWARE\gaze-grid\SignIn' -Name Profile
        $profile = [string]$shared.Profile | ConvertFrom-Json
        if ($profile.startLayout -ne 'main-14x1-top' -or $profile.keyboardScale -ne 120 -or $profile.predictionMode -ne 'off' -or $profile.learning) { throw 'Unexpected sign-in keyboard preferences.' }
    }
    # Avoid inheriting Qt paths from the build environment: verify deployed plugins.
    Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QML2_IMPORT_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QML_IMPORT_PATH -ErrorAction SilentlyContinue
    $env:QT_LOGGING_TO_CONSOLE = '1'
    $env:PATH = "$env:SystemRoot/System32;$env:SystemRoot"
    $env:GAZE_GRID_CONFIG_DIR = "$env:RUNNER_TEMP/gaze-grid-installed-test"
    if (!(Test-Path "$env:ProgramFiles/gaze-grid/keyboards/main-14x1-bottom.xml")) { throw 'Installed keyboard pack is missing.' }
    $report = "$env:RUNNER_TEMP/gaze-grid-app-$mode-diagnose.json"
    # UIAccess must launch through ShellExecute, as it would from the Start menu.
    # Redirecting stdout switches Start-Process to CreateProcess (error 740).
    Remove-Item $report -ErrorAction SilentlyContinue
    $diagnose = Start-Process $installed -Verb Open -ArgumentList @('--diagnose', '--validate-keyboards', '--diagnostic-output', "`"$report`"") -WorkingDirectory $env:TEMP -PassThru
    $null = $diagnose.Handle
    if (!$diagnose.WaitForExit(30000)) { $diagnose.Kill(); throw 'Installed diagnostics timed out.' }
    if ($diagnose.ExitCode -ne 0) { throw 'Installed diagnostics failed.' }
    $diagnostics = Get-Content $report -Raw | ConvertFrom-Json
    if ($ExpectTobii -and !$diagnostics.tobii.compiled) { throw 'Installed application was built without Tobii support.' }
    if (@(Get-ChildItem "$env:ProgramFiles/gaze-grid" -Recurse -File -Filter '*tobii*.dll').Count) { throw 'Installed package includes a forbidden Tobii runtime DLL.' }
    if (!$diagnostics.portal.tokenQuerySucceeded -or [bool]$diagnostics.portal.uiAccess -ne $expectedUIAccess) { throw "Unexpected UIAccess token in $mode installation." }
    # Launch the real Start menu entry. Advertised MSI shortcuts can invoke
    # installer repair instead of opening the application.
    $shortcutPath = Join-Path ([Environment]::GetFolderPath('Programs')) 'Gaze Grid/Gaze Grid.lnk'
    if (!(Test-Path -LiteralPath $shortcutPath)) { throw 'Start menu shortcut is missing.' }
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $null
    try {
        $shortcut = $shell.CreateShortcut($shortcutPath)
        if ($shortcut.TargetPath -ne $installed) { throw 'Start menu shortcut must target the installed executable directly.' }
        if ([IO.Path]::GetFullPath($shortcut.WorkingDirectory).TrimEnd('\', '/') -ne (Split-Path $installed)) { throw "Start menu shortcut has an incorrect working directory: $($shortcut.WorkingDirectory)" }
    } finally {
        if ($shortcut) { [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shortcut) }
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($shell)
    }
    $process = Start-Process $shortcutPath -Verb Open -ArgumentList '--smoke' -WorkingDirectory $env:TEMP -PassThru
    $null = $process.Handle
    if (!$process.WaitForExit(30000)) { $process.Kill(); throw 'Installed application timed out.' }
    if ($process.ExitCode -ne 0) { throw "Installed application failed: $($process.ExitCode)" }
    if ($ExpectLocalIme) {
        foreach ($component in @('platforminputcontexts/qtvirtualkeyboardplugin.dll','qml/QtQuick/VirtualKeyboard/qmldir','qml/QtQuick/VirtualKeyboard/Plugins/OpenWNN/qtvkbopenwnnplugin.dll','qml/QtQuick/VirtualKeyboard/Plugins/Pinyin/qtvkbpinyinplugin.dll')) {
            if (!(Test-Path (Join-Path (Split-Path $installed) $component))) { throw "Installed IME component missing: $component" }
        }
        $ime = Start-Process $installed -Verb Open -ArgumentList @('--local-ime','--smoke') -WorkingDirectory $env:TEMP -PassThru
        $null = $ime.Handle
        if (!$ime.WaitForExit(30000)) { $ime.Kill(); throw 'Installed IME startup timed out.' }
        if ($ime.ExitCode -ne 0) { throw "Installed IME startup failed: $($ime.ExitCode)" }
    }
    if ($mode -eq 'default') { Test-KeyboardInstanceLifecycle }
} finally { Run-Msi '/x' }
$remainingStartup = @(((Get-ItemProperty $accessibility -Name Configuration -ErrorAction SilentlyContinue).Configuration -split ',') | Where-Object { $_ })
Test-DesktopStartup $false
if ($atName -in $remainingStartup) { throw 'Sign-in startup remained after uninstall.' }
if (@(Compare-Object $originalStartup $remainingStartup).Count) { throw 'Uninstall changed another accessibility startup entry.' }
if (Test-Path "$accessibility\ATs\$atName") { throw 'Sign-in registration remained after uninstall.' }
if ((Test-Path "Cert:\LocalMachine\Root\$CertificateThumbprint") -ne $preexistingTrust) { throw 'Uninstall failed to remove installer-owned trust or removed pre-existing trust.' }
if (Test-Path "HKLM:\Software\gaze-grid\Certificates\Managed\$CertificateThumbprint") { throw 'Certificate ownership remained after uninstall.' }
if ($preexistingTrust) { Remove-Item "Cert:\LocalMachine\Root\$CertificateThumbprint" -Force }

}

if ($UpgradeFromMsi) { Test-StartupUpgrade }
