# Execute on an isolated Windows CI runner. Never use the persistent identity.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. "$PSScriptRoot/../platform/windows/Clear-SigningIdentity.ps1"
function Assert([bool]$Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function New-Fixture {
    $folder = Join-Path $env:RUNNER_TEMP ([guid]::NewGuid().ToString())
    New-Item -ItemType Directory $folder | Out-Null
    $pfx = Join-Path $folder 'disposable.pfx'
    $password = ConvertTo-SecureString ([guid]::NewGuid().ToString()) -AsPlainText -Force
    $created = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=GazeGrid cleanup test' `
        -CertStoreLocation Cert:\CurrentUser\My -KeyAlgorithm RSA -KeyLength 2048 `
        -Provider 'Microsoft Software Key Storage Provider' -KeyExportPolicy Exportable
    try { Export-PfxCertificate -Cert $created -FilePath $pfx -Password $password | Out-Null }
    finally {
        Remove-Item "Cert:\CurrentUser\My\$($created.Thumbprint)" -DeleteKey -ErrorAction Stop
        $created.Dispose()
    }
    $cert = Import-PfxCertificate -FilePath $pfx -Password $password -CertStoreLocation Cert:\CurrentUser\My
    $rsa = [Security.Cryptography.X509Certificates.RSACertificateExtensions]::GetRSAPrivateKey($cert)
    try { $keyName = $rsa.Key.KeyName; $provider = $rsa.Key.Provider }
    finally { $rsa.Dispose() }
    Assert ([Security.Cryptography.CngKey]::Exists($keyName, $provider)) 'Imported private key is missing from fixture.'
    return @{ Certificate = $cert; Thumbprint = $cert.Thumbprint; Pfx = $pfx; Folder = $folder; KeyName = $keyName; Provider = $provider }
}
function Assert-KeyGone($Fixture) {
    Assert (!(Test-Path "Cert:\CurrentUser\My\$($Fixture.Thumbprint)")) 'Certificate remained after cleanup.'
    Assert (![Security.Cryptography.CngKey]::Exists($Fixture.KeyName, $Fixture.Provider)) 'Private key container remained after cleanup.'
    Assert (!(Test-Path Env:PFX_BASE64)) 'PFX environment variable remained.'
    Assert (!(Test-Path Env:PFX_PASSWORD)) 'Password environment variable remained.'
}
foreach ($failPfx in @($false, $true)) {
    $fixture = New-Fixture
    $env:PFX_BASE64 = 'disposable-test-value'
    $env:PFX_PASSWORD = 'disposable-test-value'
    $pfxPath = if ($failPfx) { $fixture.Folder } else { $fixture.Pfx }
    $failure = $null
    try { Clear-GazeGridSigningIdentity -Certificate $fixture.Certificate -PfxPath $pfxPath }
    catch { $failure = $_.Exception.Message }
    finally {
        # A failed assertion must never leave this fixture's imported key behind.
        try {
            Assert-KeyGone $fixture
            if ($failPfx) {
                Assert ($failure -like 'Signing cleanup failed:*temporary PFX*') 'A PFX cleanup failure was hidden.'
                Remove-Item -LiteralPath $fixture.Pfx -Force
            } else {
                Assert (!$failure) "Cleanup unexpectedly failed: $failure"
                Assert (!(Test-Path -LiteralPath $fixture.Pfx)) 'PFX remained after successful cleanup.'
            }
        } finally {
            if (Test-Path "Cert:\CurrentUser\My\$($fixture.Thumbprint)") {
                Remove-Item "Cert:\CurrentUser\My\$($fixture.Thumbprint)" -DeleteKey -ErrorAction Stop
            }
            foreach ($name in @('PFX_BASE64', 'PFX_PASSWORD')) {
                if (Test-Path "Env:$name") { Remove-Item "Env:$name" }
            }
        }
    }
}
Write-Host 'Signing cleanup passed: imported key containers deleted, secret variables cleared, and cleanup failures reported without skipping key deletion.'
