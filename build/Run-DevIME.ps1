param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$StartSidecar
)

$ErrorActionPreference = "Stop"

$dll = Join-Path $PSScriptRoot "..\artifacts\bin\EstraIme.Tip\x64\$Configuration\EstraIme.Tip.dll"
if (-not (Test-Path $dll)) {
    throw "TIP DLL not found at $dll. Build first."
}

if ($StartSidecar) {
    & (Join-Path $PSScriptRoot "Start-Sidecar.ps1") -Configuration $Configuration
}

Write-Host "Registering TIP DLL with regsvr32"
$registration = Start-Process regsvr32.exe -ArgumentList "/s `"$dll`"" -Verb RunAs -Wait -PassThru
if ($registration.ExitCode -ne 0) {
    throw "regsvr32 failed with exit code $($registration.ExitCode)"
}

Write-Host "TIP registered. Open Windows input settings and enable EstraIME Alpha."
