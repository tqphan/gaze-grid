[CmdletBinding()]
param([string]$Destination = "$env:RUNNER_TEMP/Qt")
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
$tools = "$env:RUNNER_TEMP/qt-installer"
New-Item -ItemType Directory -Force $tools | Out-Null
Invoke-Checked python @('-m', 'venv', "$tools/venv")
$python = "$tools/venv/Scripts/python.exe"
# Wheels only, including every transitive and build dependency. Pip must reject
# new versions, missing hashes and source archives that would run build hooks.
Invoke-Checked $python @('-m', 'pip', 'install', '--disable-pip-version-check', '--require-hashes', '--only-binary=:all:', '-r', "$PSScriptRoot/qt-installer-requirements.txt")
$archive = "$tools/aqtinstall.zip"
Invoke-WebRequest 'https://codeload.github.com/miurahr/aqtinstall/zip/5a49f2b391ae8ee1d3c0dd87f8b33c1d49a4deb7' -OutFile $archive
if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne '4429213d0d01685634716c95ac133907e85bf311451ac97b8b46139499a0432d') { throw 'aqtinstall source checksum mismatch.' }
Expand-Archive -LiteralPath $archive -DestinationPath "$tools/source"
$env:SETUPTOOLS_SCM_PRETEND_VERSION_FOR_AQTINSTALL = '3.3.1.dev168'
try {
    Invoke-Checked $python @('-m', 'pip', 'install', '--no-index', '--no-deps', '--no-build-isolation', "$tools/source/aqtinstall-5a49f2b391ae8ee1d3c0dd87f8b33c1d49a4deb7")
} finally { Remove-Item Env:SETUPTOOLS_SCM_PRETEND_VERSION_FOR_AQTINSTALL }
Invoke-Checked $python @('-m', 'aqt', 'install-qt', 'windows', 'desktop', '6.12.0', 'win64_msvc2022_64', '--outputdir', $Destination, '--modules', 'qtvirtualkeyboard', 'qttasktree')
$qt = (Resolve-Path "$Destination/6.12.0/msvc2022_64").Path
"$qt/bin" | Out-File $env:GITHUB_PATH -Append
"QT_ROOT_DIR=$qt" | Out-File $env:GITHUB_ENV -Append
"Qt6_DIR=$qt/lib/cmake/Qt6" | Out-File $env:GITHUB_ENV -Append
