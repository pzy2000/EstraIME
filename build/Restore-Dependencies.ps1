param(
    [switch]$IncludeSettings
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\Bootstrap-DevEnv.ps1"

Write-Host "Restoring C++ solution packages"
msbuild "$PSScriptRoot\..\EstraIME.sln" /t:Restore /p:Configuration=Debug /p:Platform=x64

if ($IncludeSettings) {
    Write-Host "Restoring WinUI 3 settings project packages"
    msbuild "$PSScriptRoot\..\src\settings\EstraIme.Settings\EstraIme.Settings.vcxproj" /t:Restore /p:Configuration=Debug /p:Platform=x64
}

Write-Host "Fetching Rust crates"
Push-Location "$PSScriptRoot\..\sidecar"
cargo fetch
Pop-Location
