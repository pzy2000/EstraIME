param(
    [string]$InputPath = "$PSScriptRoot\..\..\data\seed\base_lexicon_seed.tsv",
    [string]$OutputPath = "$PSScriptRoot\..\..\data\generated\base_lexicon.tsv"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $InputPath)) {
    throw "Seed lexicon not found: $InputPath"
}

$items = @{}
Get-Content $InputPath -Encoding UTF8 | ForEach-Object {
    if ([string]::IsNullOrWhiteSpace($_)) {
        return
    }

    $parts = $_ -split "`t"
    if ($parts.Length -lt 3) {
        return
    }

    $key = "$($parts[0])`t$($parts[1])"
    $freq = [int]$parts[2]
    if ($items.ContainsKey($key)) {
        if ($freq -gt $items[$key].Freq) {
            $items[$key] = [PSCustomObject]@{ Pinyin = $parts[0]; Text = $parts[1]; Freq = $freq }
        }
    }
    else {
        $items[$key] = [PSCustomObject]@{ Pinyin = $parts[0]; Text = $parts[1]; Freq = $freq }
    }
}

$outputDir = Split-Path $OutputPath -Parent
New-Item -ItemType Directory -Force -Path $outputDir | Out-Null

$items.Values |
    Sort-Object Pinyin, @{ Expression = "Freq"; Descending = $true }, Text |
    ForEach-Object { "$($_.Pinyin)`t$($_.Text)`t$($_.Freq)" } |
    Set-Content -Path $OutputPath -Encoding UTF8

Write-Host "Generated lexicon:" $OutputPath
