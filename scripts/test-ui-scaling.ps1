[CmdletBinding()]
param(
	[string]$Compiler = 'cl.exe',
	[string]$GameExecutable
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\ui-scaling-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'UiScalingTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) (Join-Path $root 'tests\UiScalingTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'UI scaling tests did not compile' }
if ($GameExecutable) { & $binary $GameExecutable } else { & $binary }
if ($LASTEXITCODE -ne 0) { throw 'UI scaling tests failed' }
