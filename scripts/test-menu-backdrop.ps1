[CmdletBinding()]
param([string]$Compiler = 'g++')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'src\Components\Modules\Menu'
$output = Join-Path $root 'build\menu-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null

# The helpers have no game dependencies. Omit only the MSVC precompiled
# header in their standalone copies, keeping the implementations identical.
$files = foreach ($name in @('Stage', 'Camera', 'Cast', 'Budget', 'Turntable'))
{
	$target = Join-Path $output "$name.cpp"
	$text = [IO.File]::ReadAllText((Join-Path $source "$name.cpp"))
	[IO.File]::WriteAllText($target, $text.Replace('#include "STDInclude.hpp"', ''))
	$target
}
$binary = Join-Path $output 'menu-backdrop-tests.exe'
& $Compiler '-std=c++20' '-Wall' '-Wextra' '-Werror' '-I' $source (Join-Path $root 'tests\MenuBackdropTests.cpp') @files '-o' $binary
if ($LASTEXITCODE -ne 0) { throw 'Menu backdrop tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Menu backdrop tests failed' }
