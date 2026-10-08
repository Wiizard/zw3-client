[CmdletBinding()]
param(
	[Parameter(Mandatory = $true)][string]$From,
	[Parameter(Mandatory = $true)][string]$To
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$iconPath = Join-Path $root "src\zw3.ico"

$gameMd5 = "FC9BF305BC9F9873CB836D64934AEA33"

if (-not (Test-Path -LiteralPath $From -PathType Leaf))
{
	throw "no game exe at $From"
}

if ((Get-FileHash -LiteralPath $From -Algorithm MD5).Hash -ne $gameMd5)
{
	throw "$From is not the 2.0.13 x64 iw4mp.exe (md5 $gameMd5)"
}

Add-Type @"
using System.Runtime.InteropServices;
public static class ImageChecksum
{
	[DllImport("imagehlp.dll", CharSet = CharSet.Unicode)]
	public static extern uint MapFileAndCheckSumW(string path, out uint headerSum, out uint checkSum);
}
"@

function Get-ResourceLeaves([byte[]]$image, [int]$rsrcRaw, [int]$directory, [int[]]$path)
{
	$named = [BitConverter]::ToUInt16($image, $rsrcRaw + $directory + 12)
	$numbered = [BitConverter]::ToUInt16($image, $rsrcRaw + $directory + 14)

	for ($i = 0; $i -lt $named + $numbered; $i++)
	{
		$entry = $rsrcRaw + $directory + 16 + 8 * $i
		$id = [BitConverter]::ToUInt32($image, $entry)
		$offset = [BitConverter]::ToUInt32($image, $entry + 4)

		if ($offset -band 0x80000000)
		{
			Get-ResourceLeaves $image $rsrcRaw ([int]($offset -band 0x7FFFFFFF)) ($path + [int]$id)
		}
		else
		{
			[pscustomobject]@{
				Type = $path[0]
				Id = if ($path.Count -ge 2) { $path[1] } else { [int]$id }
				Entry = $rsrcRaw + [int]$offset
				Rva = [BitConverter]::ToUInt32($image, $rsrcRaw + [int]$offset)
				Size = [BitConverter]::ToUInt32($image, $rsrcRaw + [int]$offset + 4)
			}
		}
	}
}

$image = [System.IO.File]::ReadAllBytes($From)
$ico = [System.IO.File]::ReadAllBytes($iconPath)

$pe = [BitConverter]::ToInt32($image, 0x3C)
$sectionCount = [BitConverter]::ToUInt16($image, $pe + 6)
$optionalSize = [BitConverter]::ToUInt16($image, $pe + 20)
$rsrc = $null

for ($i = 0; $i -lt $sectionCount; $i++)
{
	$header = $pe + 24 + $optionalSize + 40 * $i
	$name = [System.Text.Encoding]::ASCII.GetString($image, $header, 8).TrimEnd([char]0)

	if ($name -eq '.rsrc')
	{
		$rsrc = @{
			Rva = [BitConverter]::ToUInt32($image, $header + 12)
			Raw = [BitConverter]::ToUInt32($image, $header + 20)
		}
	}
}

if (-not $rsrc)
{
	throw "iw4mp.exe has no .rsrc section"
}

$leaves = @(Get-ResourceLeaves $image ([int]$rsrc.Raw) 0 @())
$icons = @($leaves | Where-Object { $_.Type -eq 3 } | Sort-Object Rva)
$groups = @($leaves | Where-Object { $_.Type -eq 14 })

if ($icons.Count -ne 5 -or $groups.Count -ne 1 -or $groups[0].Size -ne 6 + 14 * 5)
{
	throw "iw4mp.exe's icon resources are not the five icons and one group expected"
}

for ($i = 1; $i -lt $icons.Count; $i++)
{
	if ($icons[$i - 1].Rva + $icons[$i - 1].Size -ne $icons[$i].Rva)
	{
		throw "iw4mp.exe's icons are not contiguous"
	}
}

$spanStart = [int]$icons[0].Rva
$spanSize = [int]($icons[-1].Rva + $icons[-1].Size - $spanStart)

$wanted = @(16, 32, 48, 64, 256)
$images = @()
$imageCount = [BitConverter]::ToUInt16($ico, 4)

for ($i = 0; $i -lt $imageCount; $i++)
{
	$entry = 6 + 16 * $i
	$width = [int]$ico[$entry]

	if ($width -eq 0)
	{
		$width = 256
	}

	if ($wanted -contains $width)
	{
		$images += [pscustomobject]@{
			Entry = $entry
			Size = [BitConverter]::ToUInt32($ico, $entry + 8)
			Offset = [BitConverter]::ToUInt32($ico, $entry + 12)
		}
	}
}

if ($images.Count -ne 5)
{
	throw "zw3.ico does not hold the 16, 32, 48, 64 and 256 images"
}

$total = ($images | Measure-Object -Property Size -Sum).Sum

if ($total -gt $spanSize)
{
	throw "zw3.ico's five images are $total bytes, the icon span is $spanSize"
}

$spanRaw = [int]($rsrc.Raw + $spanStart - $rsrc.Rva)
[Array]::Clear($image, $spanRaw, $spanSize)

$groupRaw = [int]($rsrc.Raw + $groups[0].Rva - $rsrc.Rva)
$cursor = $spanStart
$byId = @($icons | Sort-Object Id)

for ($i = 0; $i -lt 5; $i++)
{
	$picked = $images[$i]
	$slot = $byId[$i]

	[Array]::Copy($ico, [int]$picked.Offset, $image, [int]($rsrc.Raw + $cursor - $rsrc.Rva), [int]$picked.Size)
	[BitConverter]::GetBytes([uint32]$cursor).CopyTo($image, $slot.Entry)
	[BitConverter]::GetBytes([uint32]$picked.Size).CopyTo($image, $slot.Entry + 4)

	$groupEntry = $groupRaw + 6 + 14 * $i
	[Array]::Copy($ico, [int]$picked.Entry, $image, $groupEntry, 12)
	[BitConverter]::GetBytes([uint16]$slot.Id).CopyTo($image, $groupEntry + 12)

	$cursor += [int]$picked.Size
}

[BitConverter]::GetBytes([uint16]5).CopyTo($image, $groupRaw + 4)
[System.IO.File]::WriteAllBytes($To, $image)

$headerSum = [uint32]0
$checkSum = [uint32]0

if ([ImageChecksum]::MapFileAndCheckSumW($To, [ref]$headerSum, [ref]$checkSum) -ne 0)
{
	throw "could not checksum $To"
}

[BitConverter]::GetBytes($checkSum).CopyTo($image, $pe + 24 + 64)
[System.IO.File]::WriteAllBytes($To, $image)
