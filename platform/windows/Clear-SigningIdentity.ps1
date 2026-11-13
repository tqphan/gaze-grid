function Clear-GazeGridSigningIdentity {
    [CmdletBinding()]
    param(
        [System.Security.Cryptography.X509Certificates.X509Certificate2]$Certificate,
        [Parameter(Mandatory)][string]$PfxPath
    )
    # Attempt every cleanup even if another fails. Do not hide a failure or let
    # the signing job publish artifacts before cleanup has succeeded.
    $failures = [Collections.Generic.List[string]]::new()
    foreach ($name in @('PFX_BASE64', 'PFX_PASSWORD')) {
        try {
            if (Test-Path "Env:$name") { Remove-Item "Env:$name" -ErrorAction Stop }
        } catch { $failures.Add("environment $name`: $($_.Exception.Message)") }
    }
    try {
        if (Test-Path -LiteralPath $PfxPath -ErrorAction Stop) {
            if ((Get-Item -LiteralPath $PfxPath -ErrorAction Stop).PSIsContainer) { throw 'Expected a PFX file, not a directory.' }
            Remove-Item -LiteralPath $PfxPath -Force -ErrorAction Stop
            if (Test-Path -LiteralPath $PfxPath -ErrorAction Stop) { throw 'PFX file remains.' }
        }
    } catch { $failures.Add("temporary PFX: $($_.Exception.Message)") }
    if ($Certificate) {
        try {
            $certificatePath = "Cert:\CurrentUser\My\$($Certificate.Thumbprint)"
            Remove-Item -Path $certificatePath -DeleteKey -ErrorAction Stop
            if (Test-Path $certificatePath -ErrorAction Stop) { throw 'Certificate remains.' }
        } catch { $failures.Add("signing certificate/private key: $($_.Exception.Message)") }
        finally { $Certificate.Dispose() }
    }
    if ($failures.Count) { throw "Signing cleanup failed: $($failures -join '; ')" }
}
