[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\remix-scene-completion-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Remix\FixedFunction.cpp'))
$start = $source.IndexOf('void DrawCulledBatches(IDirect3DDevice9* device)', [StringComparison]::Ordinal)
if ($start -lt 0) { throw 'Missing scene completion function' }
$end = $source.IndexOf('{', $start) + 1
$depth = 1
while ($depth -gt 0) {
    if ($source[$end] -eq '{') { $depth++ }
    if ($source[$end] -eq '}') { $depth-- }
    $end++
}
[IO.File]::WriteAllText((Join-Path $output 'SceneCompletion.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'RemixSceneCompletionTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\RemixSceneCompletionTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Scene completion tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Scene completion tests failed' }
