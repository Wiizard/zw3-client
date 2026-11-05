[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\matchmaking-flow-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\ZWNet.cpp'))
foreach ($part in @(
  @{ Name = 'MatchUiGuard.inc'; Start = 'static std::atomic<std::uint64_t> matchUiSnapshotGeneration'; End = 'struct LobbyMemberSnapshot' },
  @{ Name = 'VoteMapImage.inc'; Start = 'static std::string ResolveVoteMapImage('; End = 'static std::string ResolveVoteMapDisplayName(' },
  @{ Name = 'VoteDvars.inc'; Start = 'void ZWNet::UpdateVoteDvars('; End = 'static void ServerStatusText(' },
  @{ Name = 'VoteSubmit.inc'; Start = 'void ZWNet::VoteMap('; End = 'void ZWNet::CancelSearch(' }
)) {
  $start = $source.IndexOf($part.Start, [StringComparison]::Ordinal)
  $end = $source.IndexOf($part.End, $start, [StringComparison]::Ordinal)
  if ($start -lt 0 -or $end -le $start) { throw "Missing production function for $($part.Name)" }
  [IO.File]::WriteAllText((Join-Path $output $part.Name), $source.Substring($start, $end - $start))
}
$binary = Join-Path $output 'MatchmakingFlowTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) ('/I' + (Join-Path $root 'deps\json\single_include')) (Join-Path $root 'tests\MatchmakingFlowTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Matchmaking flow tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Matchmaking flow tests failed' }
