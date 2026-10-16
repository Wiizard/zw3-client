[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$buildDirectory = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $buildDirectory | Out-Null

# Preserve the familiar build/zw3.sln entry point while referencing the x64 projects.
$solution = [System.IO.File]::ReadAllText((Join-Path $root 'zw3.sln'))
$solution = $solution.Replace('"projects\', '"..\projects\')
[System.IO.File]::WriteAllText((Join-Path $buildDirectory 'zw3.sln'), $solution)

# Upgrade the old solution entry point so existing Visual Studio shortcuts use x64.
$legacySolution = Join-Path $buildDirectory 'iw4x.sln'
$legacyBackup = Join-Path $buildDirectory 'iw4x-win32.sln.backup'
if ((Test-Path -LiteralPath $legacySolution) -and
    [System.IO.File]::ReadAllText($legacySolution).Contains('Debug|Win32') -and
    -not (Test-Path -LiteralPath $legacyBackup))
{
    Copy-Item -LiteralPath $legacySolution -Destination $legacyBackup
}
[System.IO.File]::WriteAllText($legacySolution, $solution)
Write-Host 'x64 solution ready: build\zw3.sln (Debug / Release x64)'
Write-Host 'Compatibility entry point ready: build\iw4x.sln (x64). Reload any open solution in Visual Studio.'
