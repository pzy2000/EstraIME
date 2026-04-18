param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$candle = Get-Command candle.exe -ErrorAction SilentlyContinue
$light = Get-Command light.exe -ErrorAction SilentlyContinue

if (-not $candle -or -not $light) {
    throw "WiX Toolset candle.exe/light.exe not found. Install WiX before packaging."
}

$wixRoot = Join-Path $PSScriptRoot "..\installer\wix"
$outRoot = Join-Path $PSScriptRoot "..\artifacts\installer"
New-Item -ItemType Directory -Force -Path $outRoot | Out-Null

Push-Location $wixRoot
candle.exe Product.wxs Bundle.wxs -out "$outRoot\"
light.exe "$outRoot\Product.wixobj" "$outRoot\Bundle.wixobj" -out "$outRoot\EstraIME-Alpha.exe"
Pop-Location
