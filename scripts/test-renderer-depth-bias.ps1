[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$GameExecutable,
    [string]$Compiler = 'cl.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\renderer-depth-bias-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'RendererDepthBiasTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ( '/I' + (Join-Path $root 'src')) (Join-Path $root 'tests\RendererDepthBiasTests.cpp') ( '/Fo' + $output + '\') ( '/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Renderer depth-bias tests did not compile' }
& $binary $GameExecutable
if ($LASTEXITCODE -ne 0) { throw 'Renderer depth-bias tests failed' }
