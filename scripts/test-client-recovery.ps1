[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\client-recovery-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
foreach ($part in @(
    @{ File = 'SPLoadscreens.cpp'; Name = 'MenuReady.inc'; Start = 'static bool IsMainMenuOpen('; End = 'static void RestoreMenus(' },
    @{ File = 'ZWNet.cpp'; Name = 'FilterIndices.inc'; Start = 'static std::vector<std::size_t> QuickPlayFilterIndicesLocked('; End = 'static void LoadQuickPlayFilterLocked(' },
    @{ File = 'ZWNet.cpp'; Name = 'FilterHover.inc'; Start = 'void ZWNet::HighlightQuickPlayFilterSlot('; End = 'void ZWNet::ToggleQuickPlayFilterSlot(' },
    @{ File = 'ZWNet.cpp'; Name = 'ReleaseFailedMatch.inc'; Start = 'bool ZWNet::ReleaseFailedMatch('; End = 'void ZWNet::CloseOnlineSession(' },
    @{ File = 'Window.cpp'; Name = 'InputActivation.inc'; Start = 'bool Window::RefreshInputActivation('; End = 'void Window::ApplyDisplayModeDvars(' },
    @{ File = 'RawMouse.cpp'; Name = 'InputFrame.inc'; Start = 'void RawMouse::IN_Frame('; End = 'BOOL RawMouse::IN_ClipCursor(' },
    @{ File = 'Party.cpp'; Name = 'PrivateReturn.inc'; Start = 'void Party::SchedulePrivateLobbyReturn('; End = 'static bool SetStringIfChanged(' }
)) {
    $source = [IO.File]::ReadAllText((Join-Path $root ('src\Components\Modules\' + $part.File)))
    $start = $source.IndexOf($part.Start, [StringComparison]::Ordinal)
    $end = $source.IndexOf($part.End, $start, [StringComparison]::Ordinal)
    if ($start -lt 0 -or $end -le $start) { throw "Missing production function: $($part.Name)" }
    [IO.File]::WriteAllText((Join-Path $output $part.Name), $source.Substring($start, $end - $start))
}
$binary = Join-Path $output 'ClientRecoveryTests.exe'
& $Compiler /nologo /EHsc /W4 /WX /std:c++20 /MT ('/I' + $output) ('/I' + (Join-Path $root 'src')) ('/I' + (Join-Path $root 'deps\json\single_include')) (Join-Path $root 'tests\ClientRecoveryTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Client recovery tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Client recovery tests failed' }
