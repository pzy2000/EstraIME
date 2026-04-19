param(
    [ValidateSet("Release")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$stageRoot = Join-Path $repoRoot "artifacts\package-root\$Configuration"
$wixRoot = Join-Path $repoRoot "installer\wix"
$installerOut = Join-Path $repoRoot "artifacts\installer"
$cargoInstallerTarget = Join-Path $repoRoot "artifacts\cargo-installer-target"

function Require-Tool($toolName) {
    $tool = Get-Command $toolName -ErrorAction SilentlyContinue
    if (-not $tool) {
        throw "$toolName not found. Install WiX Toolset before packaging."
    }
}

function Assert-LastExitCode($step) {
    if ($LASTEXITCODE -ne 0) {
        throw "$step failed with exit code $LASTEXITCODE"
    }
}

& (Join-Path $PSScriptRoot "Build-All.ps1") -Configuration $Configuration -IncludeSettings -SkipRust
Assert-LastExitCode "Build-All.ps1"

Push-Location (Join-Path $repoRoot "sidecar")
$env:CARGO_TARGET_DIR = $cargoInstallerTarget
cargo build --release -p sidecar-daemon
Assert-LastExitCode "cargo build --release -p sidecar-daemon"
Pop-Location

& (Join-Path $repoRoot "tools\lexicon-builder\Build-Lexicon.ps1") -MaxEntries 140000 -MinFreq 5
Assert-LastExitCode "Build-Lexicon.ps1"

$tipProject = Join-Path $repoRoot "src\tip\EstraIme.Tip\EstraIme.Tip.vcxproj"
msbuild $tipProject /m /p:Configuration=$Configuration /p:Platform=x64 /p:TargetName=EstraIme.Tip.PublicAlpha
Assert-LastExitCode "msbuild EstraIme.Tip.PublicAlpha"

Remove-Item $stageRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $stageRoot "data\generated") | Out-Null
New-Item -ItemType Directory -Force -Path $installerOut | Out-Null

$copyMap = @(
    @{ Source = Join-Path $repoRoot "artifacts\bin\EstraIme.Tip\x64\$Configuration\EstraIme.Tip.PublicAlpha.dll"; Target = Join-Path $stageRoot "EstraIme.Tip.dll" },
    @{ Source = Join-Path $repoRoot "artifacts\bin\EstraIme.Settings\x64\$Configuration\EstraIme.Settings.exe"; Target = Join-Path $stageRoot "EstraIme.Settings.exe" },
    @{ Source = Join-Path $repoRoot "artifacts\bin\EstraIme.Settings\x64\$Configuration\Microsoft.WindowsAppRuntime.Bootstrap.dll"; Target = Join-Path $stageRoot "Microsoft.WindowsAppRuntime.Bootstrap.dll" },
    @{ Source = Join-Path $repoRoot "artifacts\bin\EstraIme.Settings\x64\$Configuration\WebView2Loader.dll"; Target = Join-Path $stageRoot "WebView2Loader.dll" },
    @{ Source = Join-Path $repoRoot "artifacts\bin\EstraIme.Settings\x64\$Configuration\Microsoft.Web.WebView2.Core.dll"; Target = Join-Path $stageRoot "Microsoft.Web.WebView2.Core.dll" },
    @{ Source = Join-Path $repoRoot "artifacts\bin\EstraIme.Settings\x64\$Configuration\Microsoft.Web.WebView2.Core.Projection.dll"; Target = Join-Path $stageRoot "Microsoft.Web.WebView2.Core.Projection.dll" },
    @{ Source = Join-Path $cargoInstallerTarget "release\sidecar-daemon.exe"; Target = Join-Path $stageRoot "sidecar-daemon.exe" },
    @{ Source = Join-Path $repoRoot "data\generated\base_lexicon.tsv"; Target = Join-Path $stageRoot "data\generated\base_lexicon.tsv" },
    @{ Source = Join-Path $repoRoot "data\generated\base_lexicon.meta.json"; Target = Join-Path $stageRoot "data\generated\base_lexicon.meta.json" },
    @{ Source = Join-Path $repoRoot "installer\assets\ime-config.default.json"; Target = Join-Path $stageRoot "ime-config.default.json" },
    @{ Source = Join-Path $repoRoot "LICENSE"; Target = Join-Path $stageRoot "LICENSE.txt" },
    @{ Source = Join-Path $repoRoot "THIRD_PARTY_NOTICES.md"; Target = Join-Path $stageRoot "THIRD_PARTY_NOTICES.txt" }
)

foreach ($entry in $copyMap) {
    if (-not (Test-Path $entry.Source)) {
        throw "Missing packaging input: $($entry.Source)"
    }
    New-Item -ItemType Directory -Force -Path (Split-Path $entry.Target -Parent) | Out-Null
    Copy-Item $entry.Source $entry.Target -Force
}

Require-Tool "candle.exe"
Require-Tool "light.exe"

Push-Location $wixRoot
candle.exe Product.wxs -dStageRoot="$stageRoot" -out "$installerOut\"
Assert-LastExitCode "candle.exe"
candle.exe Bundle.wxs -dMsiPath="$(Join-Path $installerOut 'EstraIME-Alpha.msi')" -out "$installerOut\"
Assert-LastExitCode "candle.exe (bundle)"
light.exe "$installerOut\Product.wixobj" -out "$installerOut\EstraIME-Alpha.msi"
Assert-LastExitCode "light.exe (msi)"
light.exe -ext WixBalExtension "$installerOut\Bundle.wixobj" -out "$installerOut\EstraIME-Alpha.exe"
Assert-LastExitCode "light.exe"
Pop-Location

Write-Host "Installer stage root:" $stageRoot
Write-Host "Installer output:" (Join-Path $installerOut "EstraIME-Alpha.exe")
