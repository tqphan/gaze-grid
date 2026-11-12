[CmdletBinding()]
param(
    [string]$CertificateThumbprint,
    [string]$PublicCertificate,
    [switch]$PrepareOnly,
    [string]$BuildDirectory = 'build/windows',
    [string]$TobiiSdk = '',
    [switch]$LocalIme = $true
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
$root = (Resolve-Path "$PSScriptRoot/../..").Path
if ($PrepareOnly) {
    $cert = Get-PfxCertificate -FilePath $PublicCertificate
    if ($cert.HasPrivateKey) { throw 'The build runner must receive only a public certificate.' }
    $CertificateThumbprint = $cert.Thumbprint
} else {
    $cert = Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint"
    if (!$cert.HasPrivateKey) { throw 'A private signing key is required.' }
}
if ($cert.Subject -ne 'CN=GazeGrid' -or $cert.Issuer -ne $cert.Subject -or $cert.NotAfter -le (Get-Date)) {
    throw 'An unexpired self-signed CN=GazeGrid certificate is required.'
}
$signCommand = Get-Command signtool.exe -ErrorAction SilentlyContinue
$signTool = if ($signCommand) { $signCommand.Source } else { $null }
if (!$signTool) {
    $signTool = (Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" | Sort-Object FullName -Descending | Select-Object -First 1).FullName
}
$configure = @('-S', $root, '-B', $BuildDirectory, '-A', 'x64', '-DGAZE_GRID_UIACCESS=OFF', "-DGAZE_GRID_CERTIFICATE_THUMBPRINT=$CertificateThumbprint")
if ($TobiiSdk) {
    $configure += @('-DGAZE_GRID_WITH_TOBII=ON', "-DTOBII_SDK=$TobiiSdk")
} else { $configure += '-DGAZE_GRID_WITH_TOBII=OFF' }
Invoke-Checked cmake $configure
Invoke-Checked cmake @('--build', $BuildDirectory, '--config', 'Release', '--parallel')
try { Invoke-Checked ctest @('--test-dir', $BuildDirectory, '-C', 'Release', '--output-on-failure') }
finally {
    Get-ChildItem "$BuildDirectory/*-qt-test.txt" | ForEach-Object { Get-Content $_.FullName }
}
$build = (Resolve-Path $BuildDirectory).Path
New-Item -ItemType Directory -Force "$build/unsigned" | Out-Null
Copy-Item "$build/Release/gaze-grid.exe" "$build/unsigned/gaze-grid.exe" -Force
$manifestTool = Join-Path (Split-Path $signTool) 'mt.exe'
Invoke-Checked $manifestTool @("-inputresource:$build/unsigned/gaze-grid.exe;#1", "-out:$build/unsigned.manifest")
[xml]$unsignedManifest = Get-Content "$build/unsigned.manifest"
$unsignedExecution = $unsignedManifest.SelectSingleNode("//*[local-name()='requestedExecutionLevel']")
if (!$unsignedExecution -or $unsignedExecution.uiAccess -ne 'false' -or $unsignedExecution.level -ne 'asInvoker' -or
    (Get-AuthenticodeSignature "$build/unsigned/gaze-grid.exe").Status -ne 'NotSigned') {
    throw 'Fallback executable must be unsigned with asInvoker and uiAccess=false.'
}
Export-Certificate -Cert $cert -FilePath "$build/gaze-grid.cer" -Force | Out-Null
# UIAccess builds cannot be run from the ordinary build directory. Test first,
# then relink the exact application with the release manifest and sign it.
Invoke-Checked cmake @('-S', $root, '-B', $BuildDirectory, '-DGAZE_GRID_UIACCESS=ON')
Invoke-Checked cmake @('--build', $BuildDirectory, '--config', 'Release', '--target', 'gaze-grid', '--parallel')
$build = (Resolve-Path $BuildDirectory).Path
$exe = "$build/Release/gaze-grid.exe"
$manifestTool = Join-Path (Split-Path $signTool) 'mt.exe'
Invoke-Checked $manifestTool @("-inputresource:$exe;#1", "-out:$build/gaze-grid.manifest")
[xml]$manifest = Get-Content "$build/gaze-grid.manifest"
$execution = $manifest.SelectSingleNode("//*[local-name()='requestedExecutionLevel']")
if (!$execution -or $execution.uiAccess -ne 'true' -or $execution.level -ne 'asInvoker') {
    throw 'Release manifest must request asInvoker with uiAccess=true.'
}
if (!$PrepareOnly) {
    Invoke-Checked $signTool @('sign', '/sha1', $CertificateThumbprint, '/fd', 'SHA256', $exe)
    Invoke-Checked $signTool @('verify', '/pa', $exe)
}
$deploy = "$build/deploy"
if (Test-Path $deploy) {
    if (!(Resolve-Path $deploy).Path.StartsWith($build + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Deployment directory escaped the build directory.' }
    Remove-Item -LiteralPath $deploy -Recurse -Force
}
New-Item -ItemType Directory $deploy | Out-Null
# Only scan Qt-only generated QML; the source directory also contains KDE imports.
$qtDirectory = (& qmake -query QT_INSTALL_PREFIX).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Could not locate the Qt installation.' }
& "$PSScriptRoot/Deploy-Qt.ps1" -Application $exe -Directory $deploy -QtDirectory $qtDirectory -QmlDirectory "$build/windows-ui" -LocalIme:$LocalIme -NoCompilerRuntime
# --diagnose uses Qt's offscreen platform, which windeployqt omits by default.
$pluginDirectory = (& qmake -query QT_INSTALL_PLUGINS).Trim()
if ($LASTEXITCODE -ne 0) { throw 'Could not locate Qt platform plugins.' }
Copy-Item "$pluginDirectory/platforms/qoffscreen.dll" "$deploy/platforms/qoffscreen.dll"
if (@(Get-ChildItem -LiteralPath $deploy -Recurse -File -Filter '*tobii*.dll').Count) {
    throw 'Tobii runtime DLLs must not be distributed in the MSI.'
}
if ($PrepareOnly) {
    # Materialize all Qt, MSVC runtime, keyboard and license files now. The later
    # packaging runner needs neither Qt/Python nor a compiler or private key.
    Invoke-Checked cmake @('--install', $build, '--config', 'Release', '--prefix', "$build/payload")
    "exe_hash=$((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant())" | Out-File $env:GITHUB_OUTPUT -Append
    return
}
Push-Location $build
try { Invoke-Checked cpack @('-G', 'WIX', '-C', 'Release') }
finally {
    $wixLog = "$build/_CPack_Packages/win64/WIX/wix.log"
    if (Test-Path $wixLog) { Get-Content $wixLog }
    Pop-Location
}
$packages = @(Get-ChildItem "$build/*.msi")
if ($packages.Count -ne 1) { throw 'Expected exactly one MSI in the build directory.' }
foreach ($msi in $packages) {
    & "$PSScriptRoot/Assert-NoTobiiRuntime.ps1" -Msi $msi.FullName
    Invoke-Checked $signTool @('sign', '/sha1', $CertificateThumbprint, '/fd', 'SHA256', $msi.FullName)
    Invoke-Checked $signTool @('verify', '/pa', $msi.FullName)
}
Export-Certificate -Cert $cert -FilePath "$build/gaze-grid.cer" -Force | Out-Null
