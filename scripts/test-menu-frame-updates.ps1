[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\menu-frame-update-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Menus.cpp'))
$functions = foreach ($name in @('static void UpdateNewsCarousel()', 'void Menus::UpdateLoadingProgress(const bool isMainThread)')) {
    $start = $source.IndexOf($name, [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing menu update: $name" }
    $end = $source.IndexOf('{', $start) + 1
    $depth = 1
    while ($depth -gt 0) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    $source.Substring($start, $end - $start)
}
[IO.File]::WriteAllText((Join-Path $output 'MenuFrameUpdates.inc'), ($functions -join "`n"))
$binary = Join-Path $output 'MenuFrameUpdateTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) ('/I' + $output) (Join-Path $root 'tests\MenuFrameUpdateTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Menu frame update tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Menu frame update tests failed' }
