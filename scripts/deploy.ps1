[CmdletBinding()]
param(
	[string]$Configuration = "Release",
	[string]$Target = "D:\SteamLibrary\steamapps\common\Call of Duty Modern Warfare 2 211 zw3"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "build\bin\x64\$Configuration"

if (-not (Test-Path -LiteralPath $Target -PathType Container))
{
	throw "deploy target is not a directory: $Target"
}

$artifacts = @('zw3.dll', 'd3d9.dll')

foreach ($name in $artifacts)
{
	$from = Join-Path $source $name

	if (-not (Test-Path -LiteralPath $from -PathType Leaf))
	{
		throw "missing build output: $from"
	}
}

$targetPath = [System.IO.Path]::GetFullPath($Target).TrimEnd('\') + '\'
$running = Get-Process -Name 'iw4mp', 'zw3' -ErrorAction SilentlyContinue | Where-Object {
	$_.Path -and $_.Path.StartsWith($targetPath, [System.StringComparison]::OrdinalIgnoreCase)
}

if ($running)
{
	throw "the game is running (pid $($running.Id -join ', ')). Close it before deploying."
}

foreach ($name in $artifacts)
{
	$from = Join-Path $source $name
	Copy-Item -LiteralPath $from -Destination $Target -Force
	Write-Host ("deployed {0,-14} {1,9:N0} bytes" -f $name, (Get-Item -LiteralPath $from).Length)
}

$builtExe = Join-Path $source "zw3.exe"
$zw3Exe = Join-Path $Target "zw3.exe"

if (Test-Path -LiteralPath $builtExe -PathType Leaf)
{
	Copy-Item -LiteralPath $builtExe -Destination $zw3Exe -Force
	Write-Host ("deployed {0,-14} {1,9:N0} bytes" -f 'zw3.exe', (Get-Item -LiteralPath $zw3Exe).Length)
}
else
{
	& (Join-Path $PSScriptRoot "gameexe.ps1") -From (Join-Path $Target "iw4mp.exe") -To $zw3Exe
	Write-Host ("deployed {0,-14} {1,9:N0} bytes, made from the target's iw4mp.exe" -f 'zw3.exe', (Get-Item -LiteralPath $zw3Exe).Length)
}

Write-Host "deployed to $Target"
