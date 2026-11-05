[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\session-navigation-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
foreach ($part in @(
    @{File='Party.cpp'; Name='PartyRole.inc'; Start='static bool IsHostOf('; End='static bool isPrivatePartyJoin'},
    @{File='Party.cpp'; Name='CancelJoin.inc'; Start='void Party::CancelPendingJoin()'; End='bool Party::IsPrivateMatchClient()'},
    @{File='Party.cpp'; Name='ConnectCommand.inc'; Start='Command::Add("connect",'; End='Command::Add("reconnect",'},
    @{File='ZWNet.cpp'; Name='EndpointJoin.inc'; Start='bool ZWNet::PrepareEndpointJoin('; End='void ZWNet::QuitToPlayOnline()'},
    @{File='ZWNet.cpp'; Name='Quit.inc'; Start='void ZWNet::QuitToPlayOnline()'; End='void ZWNet::EnterLobby('},
    @{File='ZWNet.cpp'; Name='CloseSession.inc'; Start='void ZWNet::CloseOnlineSession('; End='bool ZWNet::ReturnToMatchmakingLobby('},
    @{File='ZWNet.cpp'; Name='ReturnMenu.inc'; Start='void ZWNet::ReturnToIdleMenu('; End='void ZWNet::HandleServerDisconnect('}
    @{File='ZWNet.cpp'; Name='CancelPreparation.inc'; Start='void ZWNet::CancelPendingJoinPreparation()'; End='static bool IsCurrentJoinPreview('}
)) {
    $source = [IO.File]::ReadAllText((Join-Path $root ('src\Components\Modules\' + $part.File)))
    $start = $source.IndexOf($part.Start, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing production function: $($part.Start)" }
    $end = $source.IndexOf($part.End, $start, [StringComparison]::Ordinal)
    if ($end -le $start) { throw "Missing function boundary: $($part.End)" }
    [IO.File]::WriteAllText((Join-Path $output $part.Name), $source.Substring($start, $end - $start))
}
$binary = Join-Path $output 'SessionNavigationTests.exe'
& $Compiler /nologo /EHsc /W4 /WX /std:c++20 /MT ('/I' + $output) ('/I' + (Join-Path $root 'deps\json\single_include')) (Join-Path $root 'tests\SessionNavigationTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Session navigation tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Session navigation tests failed' }
