[CmdletBinding()]
param([string]$Destination = 'build/tobii-sdk')
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (!$env:GAZE_GRID_TOBII_HEADERS_BASE64) { throw 'Configure the windows-build GAZEGRID_TOBII_HEADERS_BASE64 secret with the pinned headers-only archive.' }
$manifest = Get-Content "$PSScriptRoot/tobii-sdk.json" -Raw | ConvertFrom-Json
$bytes = [Convert]::FromBase64String($env:GAZE_GRID_TOBII_HEADERS_BASE64)
$sha = [Security.Cryptography.SHA256]::Create()
try { $hash = ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
finally { $sha.Dispose() }
if ($hash -ne $manifest.sha256) { throw 'Tobii headers archive does not match the pinned SHA-256.' }
Add-Type -AssemblyName System.IO.Compression
$stream = [IO.MemoryStream]::new($bytes, $false)
$archive = [IO.Compression.ZipArchive]::new($stream, [IO.Compression.ZipArchiveMode]::Read)
try {
    $names = @($archive.Entries | ForEach-Object FullName)
    if (@(Compare-Object @($manifest.files) $names).Count -ne 0) { throw 'Expected only the two pinned Tobii headers; SDK binaries are forbidden.' }
    New-Item -ItemType Directory -Force "$Destination/include/tobii" | Out-Null
    foreach ($entry in $archive.Entries) {
        $inputStream = $entry.Open()
        $outputStream = [IO.File]::Create((Join-Path (Resolve-Path $Destination).Path $entry.FullName))
        try { $inputStream.CopyTo($outputStream) }
        finally { $inputStream.Dispose(); $outputStream.Dispose() }
    }
} finally { $archive.Dispose(); $stream.Dispose() }
Write-Host "Restored Tobii Stream Engine $($manifest.version) headers only."
