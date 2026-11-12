[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('exe', 'msi')][string]$Kind,
    [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedHash
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. "$PSScriptRoot/Clear-SigningIdentity.ps1"
# This script runs on a fresh hosted runner. Never build, install dependencies,
# load artifact scripts, package, or execute the signed input on this runner.
$root = (Resolve-Path "$PSScriptRoot/../..").Path
$identity = Get-Content "$PSScriptRoot/release-trust.json" -Raw | ConvertFrom-Json
$versionText = Get-Content "$root/CMakeLists.txt" -Raw
if ($versionText -notmatch 'project\(gaze-grid\s+VERSION\s+(\d+\.\d+\.\d+)') { throw 'Missing version.' }
$path = if ($Kind -eq 'exe') { "$root/build/windows/Release/gaze-grid.exe" } else { "$root/build/windows/gaze-grid-$($Matches[1])-win64.msi" }
if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $ExpectedHash) { throw 'Input differs from the producing job output.' }
if ((Get-AuthenticodeSignature -LiteralPath $path).Status -ne 'NotSigned') { throw 'Expected an unsigned signing input.' }
$signTool = (Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/signtool.exe" | Sort-Object FullName -Descending | Select-Object -First 1).FullName
$pfx = Join-Path $env:RUNNER_TEMP 'release-signing.pfx'
$cert = $null
try {
    if (!$env:PFX_BASE64 -or !$env:PFX_PASSWORD) { throw 'Missing signing identity.' }
    [IO.File]::WriteAllBytes($pfx, [Convert]::FromBase64String($env:PFX_BASE64))
    $password = ConvertTo-SecureString $env:PFX_PASSWORD -AsPlainText -Force
    $cert = Import-PfxCertificate -FilePath $pfx -Password $password -CertStoreLocation Cert:\CurrentUser\My
    # Reject certificate changes even if their display name is the same.
    if ($cert.Thumbprint -ne $identity.thumbprint -or !$cert.HasPrivateKey -or $cert.NotAfter -le (Get-Date)) { throw 'Unexpected or expired signing identity.' }
    Remove-Item -LiteralPath $pfx -Force
    Remove-Item Env:PFX_BASE64, Env:PFX_PASSWORD
    & $signTool sign /sha1 $cert.Thumbprint /fd SHA256 $path
    if ($LASTEXITCODE -ne 0) { throw 'Signing failed.' }
    $signature = Get-AuthenticodeSignature -LiteralPath $path
    # Self-signed publisher trust is deliberately not installed on this runner.
    if ($signature.SignerCertificate.Thumbprint -ne $identity.thumbprint -or $signature.Status -notin @('Valid', 'UnknownError', 'NotTrusted')) { throw 'Unexpected signature.' }
    $signedHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
} finally {
    Clear-GazeGridSigningIdentity -Certificate $cert -PfxPath $pfx
}
"signed_hash=$signedHash" | Out-File $env:GITHUB_OUTPUT -Append
