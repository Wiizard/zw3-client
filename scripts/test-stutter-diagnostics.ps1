[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\stutter-diagnostics-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'StutterDiagnosticsTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) (Join-Path $root 'tests\StutterDiagnosticsTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Stutter diagnostics tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Stutter diagnostics tests failed' }
