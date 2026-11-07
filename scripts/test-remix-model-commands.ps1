[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\remix-model-command-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Remix\FixedFunction.cpp'))
$start = $source.IndexOf('static bool HasUnbakedInstances(void** list)', [StringComparison]::Ordinal)
if ($start -lt 0) { throw 'Missing unbaked instance reader' }
$end = $source.IndexOf('{', $start) + 1
$depth = 1
while ($depth -gt 0) {
    if ($source[$end] -eq '{') { $depth++ }
    if ($source[$end] -eq '}') { $depth-- }
    $end++
}
[IO.File]::WriteAllText((Join-Path $output 'HasUnbakedInstances.inc'), $source.Substring($start, $end - $start))
# Exercise the actual draw reader without requiring a Direct3D device.
$draw = $source.IndexOf('static void DrawStaticModelGroup(', [StringComparison]::Ordinal)
if ($draw -lt 0) { throw 'Missing static-model draw reader' }
$start = $source.IndexOf('const std::uintptr_t record =', $draw, [StringComparison]::Ordinal)
$end = $source.IndexOf('const ModelBuffers* buffers =', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -lt 0) { throw 'Missing model record decoding' }
[IO.File]::WriteAllText((Join-Path $output 'ReadDrawRecord.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'RemixModelCommandTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + $output) (Join-Path $root 'tests\RemixModelCommandTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Model command tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Model command tests failed' }
