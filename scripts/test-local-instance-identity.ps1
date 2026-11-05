[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\local-instance-identity-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Auth.cpp'))
$start = $source.IndexOf('static bool IsSameMachineAddress(', [StringComparison]::Ordinal)
$end = $source.IndexOf('bool Auth::SendConnectDataStub(', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Missing production local identity policy' }
[IO.File]::WriteAllText((Join-Path $output 'LocalInstanceIdentity.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'LocalInstanceIdentityTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\LocalInstanceIdentityTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Local identity regression tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Local identity regression tests failed' }
