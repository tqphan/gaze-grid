[CmdletBinding()]
param(
    [switch]$NoBuild,
    [switch]$SkipTests,
    [string]$QtDirectory = '',
    [string]$QtRoot = 'C:\Qt'
)
$ErrorActionPreference = 'Stop'
$build = Join-Path $PSScriptRoot 'build/local-test'
if (!$NoBuild) {
    & "$PSScriptRoot/build.ps1" -LocalTest -SkipTests:$SkipTests -QtDirectory $QtDirectory -QtRoot $QtRoot
}
$executable = Join-Path $build 'gaze-grid.exe'
if (!(Test-Path $executable)) { throw 'Build the local test first: .\test-local.ps1' }
# Refuse an executable from a build configured without isolation.
if (!(Select-String -LiteralPath "$build/CMakeCache.txt" -Pattern '^GAZE_GRID_LOCAL_TEST:BOOL=(ON|TRUE)$' -Quiet)) {
    throw 'This build is not configured for local testing.'
}
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $executable
$start.Arguments = '--settings'
$start.WorkingDirectory = $build
$start.UseShellExecute = $false
$start.EnvironmentVariables['GAZE_GRID_CONFIG_DIR'] = Join-Path $build 'local-profile'
[System.Diagnostics.Process]::Start($start) | Out-Null
Write-Host "Local test launched. Test settings: $build/local-profile"
