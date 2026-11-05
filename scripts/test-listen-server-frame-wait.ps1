[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\listen-server-frame-wait-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Threading.cpp'))
$start = $source.IndexOf('static void NetSleep(', [StringComparison]::Ordinal)
$end = $source.IndexOf('int Threading_ComFrameWait(int minMsec)', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Missing production frame wait function' }
[IO.File]::WriteAllText((Join-Path $output 'NetSleep.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'ListenServerFrameWaitTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\ListenServerFrameWaitTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Listen server frame wait tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Listen server frame wait tests failed' }
