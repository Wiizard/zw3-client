[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\singleton-startup-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Singleton.cpp'))
$start = $source.IndexOf('Singleton::Singleton()', [StringComparison]::Ordinal)
if ($start -lt 0) { throw 'Missing production Singleton constructor' }
$end = $source.LastIndexOf('}')
[IO.File]::WriteAllText((Join-Path $output 'SingletonConstructor.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'SingletonStartupTests.exe'
& $Compiler /nologo /EHsc /W4 /WX /std:c++20 /MT ('/I' + $output) (Join-Path $root 'tests\SingletonStartupTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Singleton startup tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Singleton startup tests failed' }
