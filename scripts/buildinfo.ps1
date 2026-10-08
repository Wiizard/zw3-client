[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root 'build\src'

$revision = $null

try
{
	$revision = & git -C $root rev-list --count HEAD 2>$null
}
catch
{
	$revision = $null
}

if ($LASTEXITCODE -ne 0 -or -not $revision)
{
	if (Test-Path -LiteralPath (Join-Path $outDir 'version.h'))
	{
		Write-Host "buildinfo: no git repository, keeping the shipped version.h"
		exit 0
	}

	throw "buildinfo: git rev-list failed, the revision cannot be read"
}

$revision = "$revision".Trim()

$branch = (& git -C $root branch --show-current).Trim()

if (-not $branch)
{
	$branch = 'detached'
}

$versionH = Join-Path $outDir 'version.h'
$oldRevision = '(none)'
$oldBranch = '(none)'

if (Test-Path -LiteralPath $versionH)
{
	foreach ($line in Get-Content -LiteralPath $versionH)
	{
		if ($line -match '^#define REVISION (\d+)\s*$')
		{
			$oldRevision = $Matches[1]
		}
		if ($line -match '^#define GIT_BRANCH "([^"]+)"\s*$')
		{
			$oldBranch = $Matches[1]
		}
	}
}

if ($oldRevision -eq $revision -and $oldBranch -eq $branch)
{
	exit 0
}

Write-Host "buildinfo: $oldRevision -> $revision on $branch"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$header = "#define GIT_BRANCH `"$branch`"`r`n`r`n#define REVISION $revision`r`n#define REVISION_STR `"r$revision`"`r`n"
[System.IO.File]::WriteAllText($versionH, $header)
[System.IO.File]::WriteAllText((Join-Path $outDir 'version.hpp'), "#include `".\version.h`"`r`n")
