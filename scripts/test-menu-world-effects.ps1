[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\menu-world-effects-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Menu\WorldEffects.cpp'))
$functions = foreach ($name in @('Reset()', 'Draw(Engine::refdef_t& view)')) {
    $start = $source.IndexOf('void ' + $name, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing world FX function: $name" }
    $end = $source.IndexOf('{', $start) + 1
    $depth = 1
    while ($depth -gt 0) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    $source.Substring($start, $end - $start)
}
[IO.File]::WriteAllText((Join-Path $output 'MenuWorldEffects.inc'), ($functions -join "`n"))
$binary = Join-Path $output 'MenuWorldEffectsTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\MenuWorldEffectsTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Menu world FX tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Menu world FX tests failed' }
