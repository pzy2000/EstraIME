param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$ForceRestart
)

$ErrorActionPreference = "Stop"

$healthScript = Join-Path $PSScriptRoot "Test-Sidecar.ps1"
if (-not $ForceRestart) {
    try {
        & $healthScript -TimeoutMs 250 | Out-Null
        Write-Host "Sidecar already healthy"
        return
    }
    catch {
    }
}

if ($ForceRestart) {
    Get-Process sidecar-daemon -ErrorAction SilentlyContinue | Stop-Process -Force
}

$candidates = @(
    (Join-Path $PSScriptRoot "..\sidecar\target\$Configuration\sidecar-daemon.exe"),
    (Join-Path $PSScriptRoot "..\sidecar\target\debug\sidecar-daemon.exe"),
    (Join-Path $PSScriptRoot "..\sidecar\target\release\sidecar-daemon.exe")
) | Where-Object { Test-Path $_ }

if (-not $candidates) {
    throw "No built sidecar-daemon.exe found. Build Rust workspace first."
}

$sidecar = $candidates[0]
Write-Host "Starting sidecar:" $sidecar
$process = Start-Process -FilePath $sidecar -WorkingDirectory (Split-Path $sidecar) -PassThru -WindowStyle Hidden

for ($attempt = 0; $attempt -lt 30; $attempt++) {
    Start-Sleep -Milliseconds 200
    try {
        & $healthScript -TimeoutMs 250 | Out-Null
        Write-Host "Sidecar is healthy (PID $($process.Id))"
        return
    }
    catch {
    }
}

throw "Sidecar started but health check did not pass in time."
