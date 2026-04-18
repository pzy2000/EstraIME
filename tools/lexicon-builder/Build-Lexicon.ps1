param(
    [string]$OutputPath = "$PSScriptRoot\..\..\data\generated\base_lexicon.tsv",
    [string]$CacheRoot = "$PSScriptRoot\..\..\data\cache",
    [int]$MaxEntries = 120000,
    [int]$MinFreq = 5,
    [switch]$RefreshSources
)

$ErrorActionPreference = "Stop"

$venvRoot = Join-Path $PSScriptRoot ".venv"
$venvPython = Join-Path $venvRoot "Scripts\python.exe"
$requirements = Join-Path $PSScriptRoot "requirements.txt"
$builder = Join-Path $PSScriptRoot "build_lexicon.py"

$outputDir = Split-Path $OutputPath -Parent
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
New-Item -ItemType Directory -Force -Path $CacheRoot | Out-Null

$resolvedOutputDir = (Resolve-Path $outputDir).Path
$resolvedCacheRoot = (Resolve-Path $CacheRoot).Path

if (-not (Test-Path $venvPython)) {
    Write-Host "Creating lexicon-builder virtual environment"
    python -m venv $venvRoot
}

Write-Host "Installing lexicon-builder requirements"
& $venvPython -m pip install --disable-pip-version-check -r $requirements

$arguments = @(
    $builder,
    "--output", $resolvedOutputDir,
    "--output-file", (Split-Path $OutputPath -Leaf),
    "--cache-root", $resolvedCacheRoot,
    "--max-entries", "$MaxEntries",
    "--min-freq", "$MinFreq"
)

if ($RefreshSources) {
    $arguments += "--refresh-sources"
}

& $venvPython @arguments
