[CmdletBinding()]
param([string]$Compiler='cl.exe',[string]$SourceFile)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root 'build\remix-map-settings-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
if(!$SourceFile){$SourceFile=Join-Path $root 'src\Components\Modules\Remix\Core.cpp'}
$source=[IO.File]::ReadAllText($SourceFile)
$start=$source.IndexOf('using Member =')
$end=$source.IndexOf('static HMODULE runtime', $start)
$chunks=@($source.Substring($start,$end-$start),'static constexpr const char* menuWindowKey = "menu_window";')
foreach($name in @('static std::string Trim(', 'static bool ParseBool(', 'static long ParseLong(', 'static float ParseFloat(', 'static bool TryApplyTableKey(', 'static std::string FormatSetting(', 'static std::wstring IniPath(', 'static std::wstring MapIniPath(', 'static bool TryParseIni(', 'const Config& GetConfig(', 'Config& MutableConfig(', 'bool HasUnsavedChanges(', 'void LoadConfigForMap(', 'const std::string& GetActiveMap(', 'bool HasMapConfig(', 'void SaveMapConfig(')){
    $start=$source.IndexOf($name,[StringComparison]::Ordinal)
    if($start -lt 0){throw "Missing function: $name"}
    $end=$source.IndexOf('{',$start)+1
    $depth=1
    while($depth -gt 0){if($source[$end] -eq '{'){$depth++};if($source[$end] -eq '}'){$depth--};$end++}
    $chunks+=$source.Substring($start,$end-$start)
}
[IO.File]::WriteAllText((Join-Path $output 'MapSettingsPersistence.inc'),($chunks -join "`n"))
$binary=Join-Path $output 'RemixMapSettingsPersistenceTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I'+(Join-Path $root 'src')) ('/I'+$output) (Join-Path $root 'tests\RemixMapSettingsPersistenceTests.cpp') ('/Fo'+$output+'\') ('/Fe'+$binary)
if($LASTEXITCODE -ne 0){throw 'Map settings tests did not compile'}
& $binary $output
if($LASTEXITCODE -ne 0){throw 'Map settings tests failed'}
