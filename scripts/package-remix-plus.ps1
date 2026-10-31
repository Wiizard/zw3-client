[CmdletBinding()]
param(
	# The Remix Plus build's _output folder (scripts\build.ps1 in the dxvk-remix fork) and its checkout.
	[string]$Source = "D:\Github\remixplus\_output",
	[string]$Repository = "D:\Github\remixplus",
	# The game folder whose zw3 Remix settings (remix.ini, maps, rtx.conf) go in the package.
	[string]$Game = "D:\SteamLibrary\steamapps\common\Call of Duty Modern Warfare 2 211 zw3",
	[string]$Output = (Join-Path $PSScriptRoot "..\build\ship")
)

# Packs what another developer needs for zw3's RTX mode into one zip, to extract into their game folder:
#   .trex_plus\        the Remix Plus runtime (without its .pdb/.lib/.exp)
#   rtx.conf           the Remix options zw3 ships
#   zw3_remix\         remix.ini and maps\*.ini / *.toml
# Left out on purpose: rtx-remix\mods\zw3_pbr (zw3 builds it on each machine), zw3_remix_textures.conf (rebuilt
# from each machine's materials), user.conf (Remix's per-user graphics settings), logs and dumps.

$ErrorActionPreference = "Stop"

$runtime = Join-Path $Source "d3d9.dll"

if (-not (Test-Path -LiteralPath $runtime -PathType Leaf))
{
	throw "missing Remix Plus build output: $runtime"
}

# The zip names the runtime commit, so the build must be exactly that commit: no uncommitted changes, and no source
# file edited after the build.
$dirty = git -C $Repository status --short --untracked-files=no

if ($dirty)
{
	throw "the Remix Plus checkout has uncommitted changes, commit them first so the package matches a commit:`n$($dirty -join "`n")"
}

$commit = git -C $Repository rev-parse --short HEAD
$branch = git -C $Repository branch --show-current
$builtAt = (Get-Item -LiteralPath $runtime).LastWriteTime

$newerSources = git -C $Repository diff --name-only "HEAD~1" HEAD | Where-Object {
	$path = Join-Path $Repository $_
	(Test-Path -LiteralPath $path) -and (Get-Item -LiteralPath $path).LastWriteTime -gt $builtAt
}

if ($newerSources)
{
	throw "these runtime sources changed after the build at $builtAt, rebuild first:`n$($newerSources -join "`n")"
}

foreach ($required in "rtx.conf", "zw3_remix\remix.ini")
{
	if (-not (Test-Path -LiteralPath (Join-Path $Game $required) -PathType Leaf))
	{
		throw "missing $required in $Game"
	}
}

$name = "zw3-rtx-remixplus-{0}-{1}" -f (Get-Date -Format "yyyyMMdd"), $commit
$staging = Join-Path $Output $name
$zip = Join-Path $Output "$name.zip"

if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

robocopy $Source (Join-Path $staging ".trex_plus") /MIR /XF *.pdb *.lib *.exp /NFL /NDL /NJH /NJS /NP | Out-Null

if ($LASTEXITCODE -ge 8)
{
	throw "robocopy failed with exit code $LASTEXITCODE"
}

Copy-Item -LiteralPath (Join-Path $Game "rtx.conf") -Destination $staging
New-Item -ItemType Directory -Force -Path (Join-Path $staging "zw3_remix\maps") | Out-Null
Copy-Item -LiteralPath (Join-Path $Game "zw3_remix\remix.ini") -Destination (Join-Path $staging "zw3_remix")
Get-ChildItem -LiteralPath (Join-Path $Game "zw3_remix\maps") -File |
	Where-Object { $_.Extension -in ".ini", ".toml" } |
	Copy-Item -Destination (Join-Path $staging "zw3_remix\maps")

$readme = @"
zw3 RTX (Remix Plus runtime)

Runtime: $branch @ $commit, built $($builtAt.ToString("yyyy-MM-dd HH:mm"))
Packed:  $(Get-Date -Format "yyyy-MM-dd HH:mm")

Install
1. Close the game.
2. Extract everything in this zip into the game folder (next to zw3.dll), keeping the folders.
   This replaces zw3_remix\remix.ini and the map files in zw3_remix\maps with the ones in this zip.
3. Start the game with zw3's Remix mode on. zw3_remix\remix.ini already has: remix_dll = .trex_plus\d3d9.dll

Needs an NVIDIA RTX GPU and a current NVIDIA driver.
The first time each map loads, zw3 builds its PBR textures in rtx-remix\mods\zw3_pbr, which takes a while.
INSERT opens zw3's Remix menu. zw3/logs/zw3_remix_log.txt in the game folder is the log to send with bug reports.
"@
Set-Content -LiteralPath (Join-Path $staging "README_RTX.txt") -Value $readme -Encoding utf8

# Windows' tar writes zip entries with / separators (PowerShell 5.1's ZipFile writes \, which some extractors mangle).
tar -a -c -f $zip -C $staging .trex_plus zw3_remix rtx.conf README_RTX.txt

if ($LASTEXITCODE -ne 0)
{
	throw "tar failed with exit code $LASTEXITCODE"
}

Remove-Item -LiteralPath $staging -Recurse -Force

Write-Host ("packed {0} ({1:N0} MB), runtime {2} @ {3}" -f [System.IO.Path]::GetFullPath($zip), ((Get-Item -LiteralPath $zip).Length / 1MB), $branch, $commit)

# robocopy's 1 (files copied) would otherwise be the script's exit code.
exit 0
