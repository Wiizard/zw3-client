[CmdletBinding()]
param([string]$Compiler = 'cl.exe', [string]$Assembler = 'ml64.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\dedicated-clock-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$hookObject = Join-Path $output 'ThreadingStubs.obj'
$testObject = Join-Path $output 'DedicatedClockTestsAsm.obj'
& $Assembler /nologo /c ("/Fo$hookObject") (Join-Path $root 'src\Components\Modules\ThreadingStubs.asm')
if ($LASTEXITCODE -ne 0) { throw 'Production timing hook did not assemble' }
& $Assembler /nologo /c ("/Fo$testObject") (Join-Path $root 'tests\DedicatedClockTests.asm')
if ($LASTEXITCODE -ne 0) { throw 'Timing test harness did not assemble' }
$binary = Join-Path $output 'DedicatedClockTests.exe'
& $Compiler /nologo /EHsc /W4 /WX /std:c++20 /MT (Join-Path $root 'tests\DedicatedClockTests.cpp') $hookObject $testObject ("/Fo$output\") ("/Fe$binary")
if ($LASTEXITCODE -ne 0) { throw 'Dedicated timing tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Dedicated timing tests failed' }
