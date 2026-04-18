param(
    [ValidateSet("qwen3-1.7b-q4", "qwen3-1.7b-q5", "qwen2.5-3b-q4")]
    [string]$Model = "qwen3-1.7b-q4",
    [string]$OutputDirectory = "$env:LOCALAPPDATA\\EstraIME\\models"
)

$ErrorActionPreference = "Stop"

$manifest = @{
    "qwen3-1.7b-q4" = "https://example.invalid/models/qwen3-1.7b-q4_k_m.gguf"
    "qwen3-1.7b-q5" = "https://example.invalid/models/qwen3-1.7b-q5_k_m.gguf"
    "qwen2.5-3b-q4" = "https://example.invalid/models/qwen2.5-3b-q4.gguf"
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$url = $manifest[$Model]
if (-not $url) {
    throw "Unknown model id: $Model"
}

$target = Join-Path $OutputDirectory ([IO.Path]::GetFileName($url))
Write-Host "Downloading $Model to $target"
Invoke-WebRequest -Uri $url -OutFile $target
Write-Host "Download completed"
