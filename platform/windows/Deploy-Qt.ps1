[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Application,
    [Parameter(Mandatory)][string]$Directory,
    [Parameter(Mandatory)][string]$QtDirectory,
    [Parameter(Mandatory)][string]$QmlDirectory,
    [switch]$LocalIme,
    [switch]$NoCompilerRuntime
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$Application = (Resolve-Path -LiteralPath $Application).Path
$QtDirectory = (Resolve-Path -LiteralPath $QtDirectory).Path
$root = (Resolve-Path "$PSScriptRoot/../..").Path
New-Item -ItemType Directory -Force $Directory | Out-Null
$Directory = (Resolve-Path -LiteralPath $Directory).Path
$scan = Join-Path (Split-Path (Resolve-Path -LiteralPath $QmlDirectory).Path) 'qt-deploy-qml'
New-Item -ItemType Directory -Force $scan | Out-Null
# Scan the generated Windows Main, plus dynamically loaded source components.
# Keep the optional module out of the ordinary deployment scan.
Get-ChildItem -LiteralPath $scan -File -Filter '*.qml' | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
Copy-Item -LiteralPath (Join-Path $QmlDirectory 'Main.qml') -Destination $scan
Get-ChildItem "$root/ui/*.qml" | Where-Object {
    $_.Name -ne 'Main.qml' -and ($LocalIme -or $_.Name -ne 'LocalInputMethod.qml')
} | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $scan }
$keyboardQml = Join-Path $QtDirectory 'qml/QtQuick/VirtualKeyboard'
$keyboardContext = Join-Path $QtDirectory 'plugins/platforminputcontexts/qtvirtualkeyboardplugin.dll'
if ($LocalIme -and (!(Test-Path "$keyboardQml/qmldir") -or !(Test-Path $keyboardContext))) {
    throw 'Install Qt Virtual Keyboard in the selected Qt kit before using -LocalIme.'
}
$deployArguments = @('--release', '--qmldir', $scan, '--dir', $Directory, $Application)
if ($NoCompilerRuntime) { $deployArguments = @('--no-compiler-runtime') + $deployArguments }
# Exclude the optional context plugin when local IME support is disabled.
if (!$LocalIme) { $deployArguments = @('--exclude-plugins', 'qtvirtualkeyboardplugin') + $deployArguments }
$previousPath = $env:PATH
try {
    $env:PATH = "$QtDirectory/bin;$env:PATH"
    # Windows PowerShell reports native stderr warnings as ErrorRecords. The
    # deployment tool's exit code determines whether deployment failed.
    try {
        $ErrorActionPreference = 'Continue'
        & "$QtDirectory/bin/windeployqt.exe" @deployArguments
        $deploymentExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = 'Stop' }
    if ($deploymentExit -ne 0) { throw "windeployqt failed: $deploymentExit" }
    New-Item -ItemType Directory -Force "$Directory/platforms" | Out-Null
    Copy-Item -LiteralPath "$QtDirectory/plugins/platforms/qoffscreen.dll" -Destination "$Directory/platforms" -Force
    if ($LocalIme) {
        # Layouts discover extension plugins at runtime; the import scanner alone
        # does not establish that their dictionaries and DLLs are all deployed.
        New-Item -ItemType Directory -Force "$Directory/qml/QtQuick", "$Directory/platforminputcontexts" | Out-Null
        Copy-Item -LiteralPath $keyboardQml -Destination "$Directory/qml/QtQuick" -Recurse -Force
        Copy-Item -LiteralPath $keyboardContext -Destination "$Directory/platforminputcontexts" -Force
        Get-ChildItem "$QtDirectory/bin/Qt6VirtualKeyboard*.dll" | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $Directory -Force
        }
        if (Test-Path "$QtDirectory/qtvirtualkeyboard") {
            Copy-Item -LiteralPath "$QtDirectory/qtvirtualkeyboard" -Destination $Directory -Recurse -Force
        }
    }
    Set-Content -LiteralPath "$Directory/qt.conf" -Value "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml"
} finally { $env:PATH = $previousPath }
