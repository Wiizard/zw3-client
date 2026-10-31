[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\remix-query-upload-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\Remix\Device.cpp'))
$functions = foreach ($name in @('QueryIssueHook', 'QueryGetDataHook')) {
    $start = $source.IndexOf('static HRESULT STDMETHODCALLTYPE ' + $name + '(', [StringComparison]::Ordinal)
    if ($start -lt 0) { throw "Missing $name" }
    $end = $source.IndexOf('{', $start) + 1
    $depth = 1
    while ($depth -gt 0) {
        if ($source[$end] -eq '{') { $depth++ }
        if ($source[$end] -eq '}') { $depth-- }
        $end++
    }
    $source.Substring($start, $end - $start)
}
[IO.File]::WriteAllText((Join-Path $output 'QueryHooks.inc'), ($functions -join "`n"))
$binary = Join-Path $output 'RemixQueryUploadTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) ('/I' + $output) (Join-Path $root 'tests\RemixQueryUploadTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Query upload tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Query upload tests failed' }
