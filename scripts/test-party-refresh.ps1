[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\party-refresh-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\ZWNet.cpp'))
foreach ($part in @(
  @{ Name = 'PartyRefresh.inc'; Start = 'void ZWNet::RefreshActiveParty('; End = 'void ZWNet::LeaveParty(' },
  @{ Name = 'PartyResume.inc'; Start = 'void ZWNet::ResumeParty('; End = 'void ZWNet::JoinParty(' }
)) {
  $start = $source.IndexOf($part.Start, [StringComparison]::Ordinal)
  $end = $source.IndexOf($part.End, $start, [StringComparison]::Ordinal)
  if ($start -lt 0 -or $end -le $start) { throw "Missing production function for $($part.Name)" }
  [IO.File]::WriteAllText((Join-Path $output $part.Name), $source.Substring($start, $end - $start))
}
$binary = Join-Path $output 'PartyRefreshTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) ('/I' + (Join-Path $root 'deps\json\single_include')) (Join-Path $root 'tests\PartyRefreshTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Party refresh regression tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Party refresh regression tests failed' }
