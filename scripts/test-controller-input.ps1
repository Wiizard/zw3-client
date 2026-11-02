[CmdletBinding()]
param([string]$Compiler = 'cl.exe')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\controller-input-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary = Join-Path $output 'ControllerInputTests.exe'

# Compile the actual dispatcher and mapping code; only the engine calls are
# replaced by the test executable. Keep dependency flags in sync with the DLL.
[xml]$project = Get-Content (Join-Path $root 'projects\zw3.vcxproj')
$settings = $project.Project.ItemDefinitionGroup | Where-Object { $_.Condition -like '*Release*' } | Select-Object -First 1
$arguments = @('/nologo', '/EHsc', '/W4', '/WX', '/std:c++20', '/MT')
foreach ($include in $settings.ClCompile.AdditionalIncludeDirectories.Split(';')) {
    if ($include -and -not $include.Contains('%(')) {
        $arguments += '/I' + [IO.Path]::GetFullPath((Join-Path (Join-Path $root 'projects') $include))
    }
}
foreach ($define in $settings.ClCompile.PreprocessorDefinitions.Split(';')) {
    if ($define -and -not $define.Contains('%(')) { $arguments += '/D' + $define }
}
$sources = @('tests\ControllerInputTests.cpp', 'src\Controller\Engine\Key.cpp', 'src\Controller\Mapping\Key.cpp',
    'src\Controller\Mapping\Physical.cpp', 'src\Controller\Aim\Deadzone.cpp', 'src\Controller\Context.cpp')
$objects = foreach ($source in $sources) {
    # Engine/Key.cpp and Mapping/Key.cpp need distinct object names.
    $object = Join-Path $output (($source -replace '[\\/.]', '_') + '.obj')
    & $Compiler @arguments '/c' (Join-Path $root $source) ('/Fo' + $object) | Out-Host
    if ($LASTEXITCODE -ne 0) { throw "Controller input tests did not compile: $source" }
    $object
}
& $Compiler '/nologo' @objects ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Controller input tests did not link' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Controller input tests failed' }
