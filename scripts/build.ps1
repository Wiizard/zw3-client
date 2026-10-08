[CmdletBinding()]
param(
	[string]$Configuration = "Release",
	[switch]$Rebuild
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

function Resolve-MSBuild
{
	$programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')

	if ($programFiles)
	{
		$vswhere = Join-Path $programFiles 'Microsoft Visual Studio\Installer\vswhere.exe'

		if (Test-Path -LiteralPath $vswhere)
		{
			$found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
				Select-Object -First 1

			if ($found -and (Test-Path -LiteralPath $found)) { return $found }
		}
	}

	$fallbacks = @(
		'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe',
		'C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe',
		'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe'
	)

	foreach ($candidate in $fallbacks)
	{
		if (Test-Path -LiteralPath $candidate) { return $candidate }
	}

	throw "MSBuild was not found. Install Visual Studio 2022 with the C++ workload."
}

$solution = Join-Path $root 'zw3.sln'

$msbuild = Resolve-MSBuild
Write-Host "msbuild: $msbuild"

$arguments = @($solution, "/p:Configuration=$Configuration", '/p:Platform=x64', '/m', '/v:minimal', '/nologo')

if ($Rebuild) { $arguments += '/t:Rebuild' }

& $msbuild @arguments

if ($LASTEXITCODE -ne 0) { throw "build failed with $LASTEXITCODE" }

Write-Host "build ok: $Configuration x64"

$gameExe = Join-Path $root 'game\iw4mp.exe'

if (Test-Path -LiteralPath $gameExe -PathType Leaf)
{
	$zw3Exe = Join-Path $root "build\bin\x64\$Configuration\zw3.exe"
	& (Join-Path $PSScriptRoot 'gameexe.ps1') -From $gameExe -To $zw3Exe
	Write-Host "zw3.exe: made from game\iw4mp.exe"
}
else
{
	Write-Host "zw3.exe: no game\iw4mp.exe, deploy makes it from the install"
}
