[CmdletBinding()]
param([string]$Compiler = 'cl.exe', [string]$Assembler = 'ml64.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\server-timing-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$objects = foreach ($source in @('src\Components\Modules\DedicatedStubs.asm','tests\DedicatedTimingStubsTests.asm')) {
    $object = Join-Path $output (([IO.Path]::GetFileNameWithoutExtension($source)) + '.obj')
    & $Assembler /nologo /c ('/Fo' + $object) (Join-Path $root $source) | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Assembly failed: $source" }
    $object
}
$binary = Join-Path $output 'ServerTimingTests.exe'
& $Compiler /nologo /EHsc /W4 /WX /std:c++20 /MT ('/I' + (Join-Path $root 'src')) (Join-Path $root 'tests\ServerTimingTests.cpp') @objects ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Server timing tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Server timing tests failed' }
