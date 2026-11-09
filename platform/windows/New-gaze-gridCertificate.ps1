[CmdletBinding()]
param(
    [string]$OutputDirectory,
    [securestring]$Password
)
$ErrorActionPreference = 'Stop'
# Fully parameterized CI calls must never wait for clipboard interaction.
$interactiveSetup = !$PSBoundParameters.ContainsKey('OutputDirectory') -or !$PSBoundParameters.ContainsKey('Password')
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $defaultDirectory = Join-Path $env:USERPROFILE 'gaze-grid-Signing'
    $OutputDirectory = Read-Host "Certificate output directory [$defaultDirectory]"
    if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
        $OutputDirectory = $defaultDirectory
    }
}
if ($null -eq $Password) {
    $Password = Read-Host 'Choose a PFX password (keep it for the GitHub signing secret)' -AsSecureString
}
if ($Password.Length -eq 0) {
    throw 'The PFX password must not be empty.'
}
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
if ((Test-Path "$OutputDirectory/gaze-grid.pfx") -or (Test-Path "$OutputDirectory/gaze-grid.cer")) {
    throw 'Certificate output already exists; preserve the existing signing identity.'
}
$cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=GazeGrid' `
    -FriendlyName 'gaze-grid' -CertStoreLocation Cert:\CurrentUser\My `
    -KeyAlgorithm RSA -KeyLength 3072 -HashAlgorithm SHA256 `
    -KeyExportPolicy Exportable -NotAfter (Get-Date).AddYears(3)
Export-PfxCertificate -Cert $cert -FilePath "$OutputDirectory/gaze-grid.pfx" -Password $Password | Out-Null
Export-Certificate -Cert $cert -FilePath "$OutputDirectory/gaze-grid.cer" | Out-Null
Write-Output "gaze-grid signing thumbprint: $($cert.Thumbprint)"

$pfxPath = (Resolve-Path -LiteralPath "$OutputDirectory/gaze-grid.pfx").Path
$cerPath = (Resolve-Path -LiteralPath "$OutputDirectory/gaze-grid.cer").Path
$quotedPfxPath = "'" + $pfxPath.Replace("'", "''") + "'"
Write-Host ""
Write-Host "Certificate created. Reuse this identity for future releases."
Write-Host "Private certificate: $pfxPath"
Write-Host "Public certificate:  $cerPath"
Write-Host "Keep the PFX and its password backed up securely, outside the repository."
Write-Host @'

1. Open your repository on GitHub.
   Go to Settings > Environments and create or open:
   windows-release

2. Under Environment secrets, choose Add secret and add BOTH:

   GAZEGRID_SIGNING_PFX_BASE64
     Value: the base64-encoded PFX. Use the clipboard prompt below,
     or run the displayed PowerShell command. Paste the entire value.

   GAZEGRID_SIGNING_PFX_PASSWORD
     Value: the exact password you chose when this script prompted you.
     Enter the password itself, not a SecureString or base64 value.

   These must be secrets, not environment variables. Do not upload the
   PFX or paste either secret into source files, issues, or workflow logs.

3. After pasting the PFX value, clear the clipboard in PowerShell:
   Set-Clipboard -Value ""

4. Commit and push the project's scripts and workflow files to GitHub.
   Open Actions > Signed Windows MSI > Run workflow.
   Leave Create a draft GitHub release enabled to create a versioned draft.
   After it succeeds, review the draft on the repository's Releases page
   and publish when ready. You can also download the
   gaze-grid-Windows-x64-signed artifact from the workflow run.
   It contains the signed MSI and public gaze-grid.cer certificate.
   The UIAccess executable inside the MSI is signed with the same identity.

   Use Signed Windows MSI for releases. Build and test uses a temporary
   test certificate rather than this persistent signing identity.

5. On a Windows machine, run the MSI and review its certificate explanation.
   Verify the gaze-grid certificate thumbprint before accepting trust.
   Accept installs the public certificate for all users and the signed UIAccess
   app in Program Files. Decline installs an unsigned, limited-access app that
   cannot send input to higher-privilege windows or stay on top of all windows.
   Accept is the default, including silent installs. Uninstall removes trust
   added by the installer and preserves trust present before installation.
   Administrators can revoke pre-existing trust with certlm.msc under
   Trusted Root Certification Authorities > Certificates.
   Distribute only the MSI and public CER; keep the PFX/password private.

To copy the encoded PFX later, run this command in PowerShell:
'@
Write-Host ('[Convert]::ToBase64String([IO.File]::ReadAllBytes(' + $quotedPfxPath + ')) | Set-Clipboard')
if ($interactiveSetup) {
    Write-Host ""
    $copyAnswer = Read-Host 'Copy the encoded PFX to the clipboard now? [y/N]'
    if ($copyAnswer.Trim() -match '^(y|yes)$') {
        try {
            [Convert]::ToBase64String([IO.File]::ReadAllBytes($pfxPath)) | Set-Clipboard
            Write-Host 'Copied. Paste into GAZEGRID_SIGNING_PFX_BASE64, then clear the clipboard.'
        } catch {
            Write-Warning 'The certificate was created, but clipboard copying failed. Use the command above in a desktop PowerShell session.'
        }
    }
}
