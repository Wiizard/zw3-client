[CmdletBinding()]
param([string]$Compiler = 'cl.exe')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$output = Join-Path $root 'build\playlist-image-tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$binary = Join-Path $output 'PlaylistImageTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) (Join-Path $root 'tests\PlaylistImageTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Playlist image tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Playlist image tests failed' }
$source = [IO.File]::ReadAllText((Join-Path $root 'src\Components\Modules\ZWNet.cpp'))
$start = $source.IndexOf('std::string ZWNet::ResolvePlaylistImage(', [StringComparison]::Ordinal)
$end = $source.IndexOf('void ZWNet::PublishPlaylistCatalog(', $start, [StringComparison]::Ordinal)
if ($start -lt 0 -or $end -le $start) { throw 'Missing production playlist image loader' }
[IO.File]::WriteAllText((Join-Path $output 'PlaylistImageLoader.inc'), $source.Substring($start, $end - $start))
$binary = Join-Path $output 'PlaylistImageLoaderTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I' + (Join-Path $root 'src')) ('/I' + $output) (Join-Path $root 'tests\PlaylistImageLoaderTests.cpp') ('/Fo' + $output + '\') ('/Fe' + $binary)
if ($LASTEXITCODE -ne 0) { throw 'Production playlist image loader tests did not compile' }
& $binary
if ($LASTEXITCODE -ne 0) { throw 'Production playlist image loader tests failed' }
Write-Host 'Production playlist image duplicate, close, generation, retry and eviction scenarios passed'
