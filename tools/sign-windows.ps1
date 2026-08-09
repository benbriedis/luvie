# SPDX-FileCopyrightText: Ben Briedis
# SPDX-License-Identifier: Apache-2.0
#
# Authenticode-sign the given files with the certificate in the WINDOWS_CERT_P12 environment
# variable (base64-encoded .pfx/.p12), or do nothing at all if that variable is empty.
#
# Signing has to happen at two points in a release -- over luvie.exe and the two plug-in
# modules *before* they are packaged, and over the finished setup.exe *after* -- hence a
# script rather than the same twenty lines twice in the workflow. Doing nothing when
# unconfigured is deliberate: a fork, or this repository before a certificate exists, still
# produces working artifacts, and users see a SmartScreen warning instead.
#
# Environment:
#     WINDOWS_CERT_P12       base64 of the .pfx/.p12 holding certificate and key
#     WINDOWS_CERT_PASSWORD  its export password
#
# Usage:
#     pwsh tools/sign-windows.ps1 build-dist/src/luvie.exe
#     pwsh tools/sign-windows.ps1 build-dist/*-setup.exe

param(
    [Parameter(Mandatory = $true, ValueFromRemainingArguments = $true)]
    [string[]] $Paths
)

$ErrorActionPreference = 'Stop'

if (-not $env:WINDOWS_CERT_P12) {
    Write-Host "WINDOWS_CERT_P12 not set - skipping signing"
    exit 0
}

# Resolve first, and insist on a match. A glob that expands to nothing would otherwise mean
# an unsigned release that looks like a signed one.
$files = @()
foreach ($pattern in $Paths) {
    $matched = @(Get-ChildItem -Path $pattern -File -ErrorAction SilentlyContinue)
    if ($matched.Count -eq 0) { throw "nothing to sign matches '$pattern'" }
    $files += $matched
}

$signtool = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" |
            Sort-Object FullName | Select-Object -Last 1
if (-not $signtool) { throw "signtool.exe not found in the Windows SDK" }

$pfx = Join-Path $env:RUNNER_TEMP "luvie-cert.pfx"
[IO.File]::WriteAllBytes($pfx, [Convert]::FromBase64String($env:WINDOWS_CERT_P12))
try {
    foreach ($file in $files) {
        # A timestamp is what keeps the signature valid after the certificate expires --
        # which matters more than usual now that code-signing certificates are capped at
        # roughly a year.
        & $signtool.FullName sign /f $pfx /p $env:WINDOWS_CERT_PASSWORD /fd SHA256 `
            /tr http://timestamp.digicert.com /td SHA256 $file.FullName
        if ($LASTEXITCODE -ne 0) { throw "signtool failed on $($file.FullName)" }
    }
}
finally {
    Remove-Item $pfx -ErrorAction SilentlyContinue
}
