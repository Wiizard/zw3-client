[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\remix-effect-reuse-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null

# Compile the real lookup functions in isolation so address reuse exercises their actual caches.
$functions = foreach ($entry in @(
    @('FixedFunction.cpp', 'static ScriptEffectKind GetScriptEffectKind('),
    @('FixedFunction.cpp', 'static float EyeOffsetDistance('),
    @('MotionTracker.cpp', 'static bool IsEyeOffsetLampFlare(')
)) {
    $source = [IO.File]::ReadAllText((Join-Path $root ('src\Components\Modules\Remix\' + $entry[0])))
    $start = $source.IndexOf($entry[1], [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing effect lookup: $($entry[1])" }
    $end = $source.IndexOf('{', $start) + 1
    $depth = 1
    while ($depth -gt 0) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    $source.Substring($start, $end - $start)
}
[IO.File]::WriteAllText((Join-Path $output 'EffectAssetLookups.inc'), ($functions -join "`n"))
$binary = Join-Path $output 'RemixEffectAssetReuseTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) ('/I' + $output) (Join-Path $root 'tests\RemixEffectAssetReuseTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Effect asset reuse tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Effect asset reuse tests failed' }
