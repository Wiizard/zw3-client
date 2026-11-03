[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\matchmaking-flow-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\ZWNet.cpp'))
foreach ($part in @(
    @{ Name = 'StartQuickPlay.inc'; Start = 'void ZWNet::StartQuickPlay('; End = 'std::optional<nlohmann::json> ZWNet::ApplyPartyVisibility(' },
    @{ Name = 'CancelSelection.inc'; Start = 'void ZWNet::CancelPlaylistSelection('; End = 'void ZWNet::HighlightPlaylistSlot(' },
    @{ Name = 'CancelPreparation.inc'; Start = 'void ZWNet::CancelPendingJoinPreparation('; End = 'static bool IsCurrentJoinPreview(' },
    @{ Name = 'CancelMatchmaking.inc'; Start = 'void ZWNet::CancelMatchmaking('; End = 'void ZWNet::CloseOnlineSession(' },
    @{ Name = 'ReturnLobby.inc'; Start = 'void ZWNet::ScheduleMatchmakingLobby('; End = 'void ZWNet::ReturnToIdleMenu(' },
    @{ Name = 'UpdateMatchmaking.inc'; Start = 'void ZWNet::UpdateMatchmaking('; End = 'void ZWNet::RefreshNetworkMetrics(' }
)) {
    $start = $source.IndexOf($part.Start, [StringComparison]::Ordinal)
    $end = $source.IndexOf($part.End, $start, [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw "Missing production function: $($part.Name)" }
    [IO.File]::WriteAllText((Join-Path $output $part.Name), $source.Substring($start, $end - $start))
}
$binary = Join-Path $output 'MatchmakingFlowTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) ('/I' + (Join-Path $root 'src')) ('/I' + (Join-Path $root 'deps\json\single_include')) (Join-Path $root 'tests\MatchmakingFlowTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Matchmaking flow regression tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Matchmaking flow regression tests failed' }
