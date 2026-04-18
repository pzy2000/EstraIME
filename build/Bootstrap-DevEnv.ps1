param()

$ErrorActionPreference = "Stop"

$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
if ($msbuild) {
    Write-Host "MSBuild already available:" $msbuild.Source
    return
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe not found. Install Visual Studio 2022 Build Tools first."
}

$installationPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $installationPath) {
    throw "No Visual Studio Build Tools installation with MSBuild was found."
}

$launchScript = Join-Path $installationPath "Common7\Tools\Launch-VsDevShell.ps1"
if (-not (Test-Path $launchScript)) {
    throw "Launch-VsDevShell.ps1 not found at $launchScript"
}

Write-Host "Importing VS Dev Shell from $launchScript"
. $launchScript -Arch amd64 -HostArch amd64
