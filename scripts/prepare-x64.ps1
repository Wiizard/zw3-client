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
Write-Host 'x64 solution ready: build\zw3.sln (Debug / Release x64)'
