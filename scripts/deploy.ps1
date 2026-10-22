[CmdletBinding()]
param(
	[string]$Configuration = "Release",
	[string]$Target = "D:\SteamLibrary\steamapps\common\Call of Duty Modern Warfare 2 211 zw3",
	[switch]$ValidateOnly
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root "build\bin\x64\$Configuration"

if (-not (Test-Path -LiteralPath $Target -PathType Container))
{
	throw "deploy target is not a directory: $Target"
}

$artifacts = @('zw3.dll', 'd3d9.dll')

function Assert-X64Image([string]$Path)
{
	$stream = [System.IO.File]::OpenRead($Path)
	try
	{
		$reader = [System.IO.BinaryReader]::new($stream)
		if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a PE image: $Path" }
		$stream.Position = 0x3C
		$peOffset = $reader.ReadInt32()
		if ($peOffset -lt 0 -or $peOffset + 26 -gt $stream.Length) { throw "Invalid PE header: $Path" }
		$stream.Position = $peOffset
		if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664)
		{
			throw "x64 deployment refused: $Path is not a 64-bit executable or DLL. No game files were changed."
		}
	}
	finally { $stream.Dispose() }
}

foreach ($name in $artifacts)
{
	$from = Join-Path $source $name

	if (-not (Test-Path -LiteralPath $from -PathType Leaf))
	{
		throw "missing build output: $from"
	}
	Assert-X64Image $from
}

$builtExe = Join-Path $source "zw3.exe"
$iw4Exe = Join-Path $Target "iw4mp.exe"
if (Test-Path -LiteralPath $builtExe -PathType Leaf)
{
	Assert-X64Image $builtExe
}
else
{
	if (-not (Test-Path -LiteralPath $iw4Exe -PathType Leaf))
	{
		throw "The matching 2.0.13 x64 iw4mp.exe is required in $Target. No game files were changed."
	}
	Assert-X64Image $iw4Exe
	if ((Get-FileHash -LiteralPath $iw4Exe -Algorithm MD5).Hash -ne 'FC9BF305BC9F9873CB836D64934AEA33')
	{
		throw "iw4mp.exe is not the supported 2.0.13 x64 game. No game files were changed."
	}
}

if ($ValidateOnly)
{
	Write-Host 'x64 deployment preflight passed; no game files changed.'
	return
}

$targetPath = [System.IO.Path]::GetFullPath($Target).TrimEnd('\') + '\'
$running = Get-Process -Name 'iw4mp', 'zw3' -ErrorAction SilentlyContinue | Where-Object {
	$_.Path -and $_.Path.StartsWith($targetPath, [System.StringComparison]::OrdinalIgnoreCase)
}

if ($running)
{
	throw "the game is running (pid $($running.Id -join ', ')). Close it before deploying."
}

# Build the validated launcher locally before replacing any installed DLL.
if (-not (Test-Path -LiteralPath $builtExe -PathType Leaf))
{
	& (Join-Path $PSScriptRoot "gameexe.ps1") -From $iw4Exe -To $builtExe
}

foreach ($name in $artifacts)
{
	$from = Join-Path $source $name
	Copy-Item -LiteralPath $from -Destination $Target -Force
	Write-Host ("deployed {0,-14} {1,9:N0} bytes" -f $name, (Get-Item -LiteralPath $from).Length)
}

$zw3Exe = Join-Path $Target "zw3.exe"

Copy-Item -LiteralPath $builtExe -Destination $zw3Exe -Force
Write-Host ("deployed {0,-14} {1,9:N0} bytes" -f 'zw3.exe', (Get-Item -LiteralPath $zw3Exe).Length)

Write-Host "deployed to $Target"
