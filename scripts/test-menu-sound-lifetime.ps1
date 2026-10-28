[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\menu-sound-lifetime-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Menu\Backdrop.cpp'))

# Exercise the production stop policy and zone-free boundary with simulated voices.
$functions = foreach ($name in @('music_kind (', 'forget_stopped_aliases (', 'play_menu_loop (', 'stop_menu_stream (', 'Backdrop::BeginZoneFree (', 'Backdrop::EndZoneFree (')) {
    $match = [regex]::Match($source, '(?m)^\s*(?:void )?' + [regex]::Escape($name))
    if (!$match.Success) { throw "Missing sound lifetime function: $name" }
    $start = $source.IndexOf($name, $match.Index, [StringComparison]::Ordinal)
    $end = $source.IndexOf('{', $start) + 1
    $depth = 1
    while ($depth -gt 0) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    $type = if ($name -in @('music_kind (', 'play_menu_loop (')) { 'int' } else { 'void' }
    $type + ' ' + $source.Substring($start, $end - $start)
}
[IO.File]::WriteAllText((Join-Path $output 'MenuSoundLifetime.inc'), ($functions -join "`n"))
$binary = Join-Path $output 'MenuSoundLifetimeTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\MenuSoundLifetimeTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Menu sound lifetime tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Menu sound lifetime tests failed' }
