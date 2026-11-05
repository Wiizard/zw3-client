[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\party-server-build-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Party.cpp'))
$start = $source.IndexOf('void Party::PlaylistContinue()', [StringComparison]::Ordinal)
$end = $source.IndexOf('void Party::PlaylistError(', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Missing production party connection function' }
[IO.File]::WriteAllText((Join-Path $output 'PlaylistContinue.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'PartyServerBuildTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\PartyServerBuildTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Party server build tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Party server build tests failed' }
