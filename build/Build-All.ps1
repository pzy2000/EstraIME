param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$IncludeSettings,
    [switch]$SkipCpp,
    [switch]$SkipRust
)

$ErrorActionPreference = "Stop"

& "$PSScriptRoot\Bootstrap-DevEnv.ps1"

if (-not $SkipCpp) {
    msbuild "$PSScriptRoot\..\EstraIME.sln" /m /p:Configuration=$Configuration /p:Platform=x64
    if ($IncludeSettings) {
        msbuild "$PSScriptRoot\..\src\settings\EstraIme.Settings\EstraIme.Settings.vcxproj" /m /p:Configuration=$Configuration /p:Platform=x64
    }
}

if (-not $SkipRust) {
    Push-Location "$PSScriptRoot\..\sidecar"
    cargo build --workspace
    Pop-Location
}
