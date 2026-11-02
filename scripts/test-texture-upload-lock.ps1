[CmdletBinding()]
param([string]$Compiler='cl.exe')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=Join-Path $root 'build\texture-upload-lock-tests'
New-Item -ItemType Directory -Force -Path $output | Out-Null
$binary=Join-Path $output 'TextureUploadLockTests.exe'
& $Compiler '/nologo' '/EHsc' '/W4' '/WX' '/std:c++20' '/MT' ('/I'+(Join-Path $root 'src')) (Join-Path $root 'tests\TextureUploadLockTests.cpp') ('/Fo'+$output+'\') ('/Fe'+$binary)
if($LASTEXITCODE -ne 0){throw 'Texture upload tests did not compile'}
& $binary
if($LASTEXITCODE -ne 0){throw 'Texture upload tests failed'}
