[CmdletBinding()]
param(
    [string]$QtDirectory = '',
    [string]$QtRoot = 'C:\Qt',
    [string]$TobiiSdk = '',
    [switch]$Run,
    [switch]$SkipTests,
    [switch]$LocalTest
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    # Windows PowerShell can treat native stderr warnings as terminating errors.
    $ErrorActionPreference = 'Continue'
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
Push-Location $PSScriptRoot
try {
    if (!$QtDirectory) {
        $kits = @(Get-ChildItem "$QtRoot/*/mingw_64/lib/cmake/Qt6/Qt6Config.cmake" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
        if (!$kits.Count) { throw 'No Qt MinGW kit found. Install a Qt 6 MinGW x64 kit or pass -QtDirectory.' }
        $QtDirectory = (Resolve-Path (Join-Path $kits[0].DirectoryName '../../..')).Path
    }
    $QtDirectory = (Resolve-Path $QtDirectory).Path
    $tools = Join-Path $QtRoot 'Tools'
    $compilers = @(Get-ChildItem "$tools/mingw*/bin/g++.exe" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
    if (!$compilers.Count) { throw "Install Qt's MinGW x64 compiler under $tools." }
    $compilerBin = $compilers[0].DirectoryName
    $cmake = Join-Path $tools 'CMake_64/bin/cmake.exe'
    $ctest = Join-Path $tools 'CMake_64/bin/ctest.exe'
    $ninja = Join-Path $tools 'Ninja/ninja.exe'
    foreach ($tool in @($cmake, $ctest, $ninja)) {
        if (!(Test-Path $tool)) { throw "Missing $tool. Install CMake and Ninja using Qt Maintenance Tool." }
    }
    $env:PATH = "$QtDirectory/bin;$compilerBin;$env:PATH"
    $build = Join-Path $PSScriptRoot 'build/windows-mingw'
    if ($LocalTest) { $build = Join-Path $PSScriptRoot 'build/local-test' }
    $configure = @('-S', $PSScriptRoot, '-B', $build, '-G', 'Ninja', "-DCMAKE_MAKE_PROGRAM=$ninja",
        "-DCMAKE_C_COMPILER=$compilerBin/gcc.exe", "-DCMAKE_CXX_COMPILER=$compilerBin/g++.exe",
        "-DCMAKE_PREFIX_PATH=$QtDirectory", '-DGAZE_GRID_UIACCESS=OFF', '-DCMAKE_BUILD_TYPE=Release')
    $configure += "-DGAZE_GRID_LOCAL_TEST=$($LocalTest.IsPresent.ToString().ToUpperInvariant())"
    if ($LocalTest) { $TobiiSdk = '' }
    if (!$TobiiSdk -and !$LocalTest) {
        $sdkHeaders = @(Get-ChildItem "$PSScriptRoot/../stream_engine_windows_x64_*/include/tobii/tobii.h" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending)
        if ($sdkHeaders.Count) { $TobiiSdk = (Resolve-Path (Join-Path $sdkHeaders[0].DirectoryName '../..')).Path }
    }
    if ($TobiiSdk) { $configure += @('-DGAZE_GRID_WITH_TOBII=ON', "-DTOBII_SDK=$TobiiSdk") }
    else { $configure += '-DGAZE_GRID_WITH_TOBII=OFF' }
    Invoke-Checked $cmake $configure
    Invoke-Checked $cmake @('--build', $build, '--parallel', '6')
    New-Item -ItemType Directory -Force "$build/platforms" | Out-Null
    Copy-Item "$QtDirectory/plugins/platforms/qoffscreen.dll", "$QtDirectory/plugins/platforms/qwindows.dll" "$build/platforms" -Force
    Set-Content "$build/qt.conf" "[Paths]`nPrefix=.`nPlugins=.`nQmlImports=qml"
    # Deploy runtime DLLs and QML modules so Explorer can launch the executable.
    Invoke-Checked "$QtDirectory/bin/windeployqt.exe" @('--release', '--qmldir', "$build/windows-ui", "$build/gaze-grid.exe")
    # Repair incomplete copies left by an interrupted Qt deployment.
    Get-ChildItem "$build/*.dll" | Where-Object Length -eq 0 | ForEach-Object {
        $source = Join-Path "$QtDirectory/bin" $_.Name
        if (!(Test-Path $source)) { throw "Empty deployed DLL: $($_.FullName)" }
        Copy-Item -LiteralPath $source -Destination $_.FullName -Force
    }
    New-Item -ItemType Directory -Force "$build/keyboards" | Out-Null
    Copy-Item "$PSScriptRoot/references/optikey-keyboard-en-us/*" "$build/keyboards" -Recurse -Force
    if (!$SkipTests) { Invoke-Checked $ctest @('--test-dir', $build, '--output-on-failure') }
    # Exercise the actual Windows platform, beyond the offscreen test suite.
    $startup = New-Object System.Diagnostics.ProcessStartInfo
    $startup.FileName = "$build/gaze-grid.exe"
    $startup.Arguments = '--smoke'
    $startup.WorkingDirectory = $env:TEMP
    $startup.UseShellExecute = $false
    $startup.EnvironmentVariables['QT_QPA_PLATFORM'] = 'windows'
    $startup.EnvironmentVariables.Remove('QT_PLUGIN_PATH')
    $check = [System.Diagnostics.Process]::Start($startup)
    if (!$check.WaitForExit(15000)) { $check.Kill(); throw 'Windows startup check timed out.' }
    if ($check.ExitCode -ne 0) { throw "Windows startup check failed: $($check.ExitCode)" }
    Write-Host "Built: $build/gaze-grid.exe"
    if ($Run) { Start-Process -FilePath "$build/gaze-grid.exe" -WorkingDirectory $PSScriptRoot }
} finally { Pop-Location }
