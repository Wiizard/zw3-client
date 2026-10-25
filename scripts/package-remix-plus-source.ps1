[CmdletBinding()]
param(
	[string]$Repository = "D:\Github\remixplus",
	[string]$Output = (Join-Path $PSScriptRoot "..\build\ship")
)

# Packs the Remix Plus source at its current commit for other developers, two ways:
#   remixplus-zw3-source-<commit>-full.zip  everything needed to build, submodules included (~1 GB, mostly SDKs)
#   remixplus-zw3-source-<commit>.zip       the same without the big SDK submodules (dlss, FidelityFX-SDK, xess, nrc);
#                                           fetch-submodules.ps1 inside downloads them at the pinned commits
# Only tracked files go in (git archive), so build output and local files never do. Agent instructions (.claude,
# .agents, CLAUDE.md, AGENTS.md, .coderabbit.yml), CI (.github) and docs are left out.

$ErrorActionPreference = "Stop"

$dirty = git -C $Repository status --short --untracked-files=no

if ($dirty)
{
	throw "the Remix Plus checkout has uncommitted changes, commit them first so the zip matches a commit:`n$($dirty -join "`n")"
}

$commit = git -C $Repository rev-parse --short HEAD
$branch = git -C $Repository branch --show-current
$name = "remixplus-zw3-source-$commit"
$staging = Join-Path $env:TEMP $name
$bigSubmodules = "submodules/dlss", "submodules/FidelityFX-SDK", "submodules/xess", "submodules/nrc"

New-Item -ItemType Directory -Force -Path $Output | Out-Null
if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging -Recurse -Force }
New-Item -ItemType Directory -Force -Path $staging | Out-Null

function Expand-Archive-Of([string]$directory, [string]$prefix)
{
	$tarFile = Join-Path $env:TEMP "$name-part.tar"
	if ($prefix) { git -C $directory archive --format=tar --prefix=$prefix -o $tarFile HEAD }
	else { git -C $directory archive --format=tar -o $tarFile HEAD }
	tar -x -f $tarFile -C $staging
	Remove-Item -LiteralPath $tarFile
}

Expand-Archive-Of $Repository ""

# Submodules at the commits this checkout pins; a zip has no .git, so git can't fetch them later.
$submodules = foreach ($line in git -C $Repository submodule status)
{
	$fields = $line.Trim().TrimStart('+', '-', 'U') -split '\s+'
	$url = git -C $Repository config --file .gitmodules --get "submodule.$($fields[1]).url"
	[pscustomobject]@{ Path = $fields[1]; Commit = $fields[0]; Url = $url }
	Expand-Archive-Of (Join-Path $Repository $fields[1]) "$($fields[1])/"
}

foreach ($drop in ".agents", ".claude", "AGENTS.md", "CLAUDE.md", ".coderabbit.yml", "docs", "documentation", "examples")
{
	$path = Join-Path $staging $drop
	if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse -Force }
}

Get-ChildItem -LiteralPath $staging -Recurse -Force -Directory |
	Where-Object { $_.Name -in ".github", ".claude", ".agents", ".cursor", ".vscode", ".idea" } |
	Sort-Object { $_.FullName.Length } -Descending |
	Remove-Item -Recurse -Force
Get-ChildItem -LiteralPath $staging -Recurse -Force -File |
	Where-Object { $_.Name -in "CLAUDE.md", "AGENTS.md" } |
	Remove-Item -Force

$readme = @"
Remix Plus runtime source used by zw3

Branch $branch @ $commit (xoxor4d's game/cod_plus with zw3's runtime changes on top).
Build: see README.md (scripts\build.ps1); the built runtime goes in the game's .trex_plus folder.
Lean zip only: run fetch-submodules.ps1 first (needs git) to download the SDK submodules it leaves out.
"@
Set-Content -LiteralPath (Join-Path $staging "README_ZW3.txt") -Value $readme -Encoding utf8

function Write-Zip([string]$zip)
{
	if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
	$items = Get-ChildItem -LiteralPath $staging -Force | ForEach-Object { $_.Name }
	tar -a -c -f $zip -C $staging @items

	if ($LASTEXITCODE -ne 0)
	{
		throw "tar failed with exit code $LASTEXITCODE"
	}

	Write-Host ("packed {0} ({1:N0} MB)" -f [System.IO.Path]::GetFullPath($zip), ((Get-Item -LiteralPath $zip).Length / 1MB))
}

Write-Zip (Join-Path $Output "$name-full.zip")

# The lean zip: the big SDKs come back through a script that clones each at its pinned commit.
$fetchLines = foreach ($submodule in $submodules | Where-Object { $_.Path -in $bigSubmodules })
{
	Remove-Item -LiteralPath (Join-Path $staging $submodule.Path) -Recurse -Force
	"Fetch '$($submodule.Path)' '$($submodule.Url)' '$($submodule.Commit)'"
}

$fetch = @"
# Downloads the SDK submodules the lean source zip leaves out, at the commits this source pins.
`$ErrorActionPreference = "Stop"

function Fetch([string]`$path, [string]`$url, [string]`$commit)
{
	`$target = Join-Path `$PSScriptRoot `$path
	if (Test-Path -LiteralPath `$target) { Remove-Item -LiteralPath `$target -Recurse -Force }
	git init -q `$target
	git -C `$target fetch -q --depth 1 `$url `$commit
	git -C `$target checkout -q FETCH_HEAD
	Write-Host "fetched `$path @ `$commit"
}

$($fetchLines -join "`n")
"@
Set-Content -LiteralPath (Join-Path $staging "fetch-submodules.ps1") -Value $fetch -Encoding utf8

Write-Zip (Join-Path $Output "$name.zip")
Remove-Item -LiteralPath $staging -Recurse -Force

exit 0
