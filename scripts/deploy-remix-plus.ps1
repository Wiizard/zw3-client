[CmdletBinding()]
param(
	# The Remix Plus build's _output folder (scripts\build.ps1 in the dxvk-remix fork).
	[string]$Source = "D:\Github\remixplus\_output",
	[string]$Target = "D:\SteamLibrary\steamapps\common\Call of Duty Modern Warfare 2 211 zw3",
	[switch]$ValidateOnly
)

# Installs the Remix Plus runtime beside the stock one, in <game>\.trex_plus. The stock .trex folder is left alone.
# zw3 loads it once zw3_remix\remix.ini has: remix_dll = .trex_plus\d3d9.dll

$ErrorActionPreference = "Stop"

if (-not (Test-Path -LiteralPath $Target -PathType Container))
{
	throw "deploy target is not a directory: $Target"
}

$runtime = Join-Path $Source "d3d9.dll"

if (-not (Test-Path -LiteralPath $runtime -PathType Leaf))
{
	throw "missing Remix Plus build output: $runtime"
}

$stream = [System.IO.File]::OpenRead($runtime)
try
{
	$reader = [System.IO.BinaryReader]::new($stream)
	if ($reader.ReadUInt16() -ne 0x5A4D) { throw "Not a PE image: $runtime" }
	$stream.Position = 0x3C
	$stream.Position = $reader.ReadInt32()
	if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x8664)
	{
		throw "x64 deployment refused: $runtime is not a 64-bit DLL. No game files were changed."
	}
}
finally { $stream.Dispose() }

if ($ValidateOnly)
{
	Write-Host 'Remix Plus deployment preflight passed; no game files changed.'
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

$destination = Join-Path $Target ".trex_plus"

# /MIR keeps .trex_plus an exact copy of the build, so files dropped from a later build don't linger.
robocopy $Source $destination /MIR /NFL /NDL /NJH /NP | Out-Null

if ($LASTEXITCODE -ge 8)
{
	throw "robocopy failed with exit code $LASTEXITCODE"
}

Write-Host ("deployed {0} files to {1}" -f (Get-ChildItem -LiteralPath $destination -Recurse -File).Count, $destination)
Write-Host 'zw3 uses it when zw3_remix\remix.ini has: remix_dll = .trex_plus\d3d9.dll'
