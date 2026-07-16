[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Map,

    [string]$Root = 'D:\SteamLibrary\steamapps\common\ZW3 WIP',

    [string]$FsGame,

    [switch]$Deploy
)

$ErrorActionPreference = 'Stop'

if ($Map.IndexOf('zombie', [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
    throw "Only zones whose names contain 'zombie' can use the protected format: $Map"
}

if (-not $FsGame) {
    $FsGame = "mods/$Map"
}

$executable = Join-Path $Root 'zw3.exe'
$output = Join-Path $Root "zonebuilder_out\$Map.ff"
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$logDirectory = Join-Path $Root "zonebuilder_logs\protected_${Map}_$timestamp"
$stdout = Join-Path $logDirectory 'stdout.log'
$stderr = Join-Path $logDirectory 'stderr.log'

if (-not (Test-Path -LiteralPath $executable)) {
    throw "ZW3 executable not found: $executable"
}

New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null

$arguments = @(
    '-zonebuilder',
    '-stdout',
    '+set', 'fs_game', $FsGame,
    '+buildzone', $Map,
    '+quit'
)

$process = Start-Process -FilePath $executable `
    -ArgumentList $arguments `
    -WorkingDirectory $Root `
    -WindowStyle Hidden `
    -RedirectStandardOutput $stdout `
    -RedirectStandardError $stderr `
    -PassThru `
    -Wait

if (-not (Test-Path -LiteralPath $output)) {
    throw "ZoneBuilder did not create: $output"
}

$stream = [System.IO.File]::OpenRead($output)
try {
    $header = New-Object byte[] 4
    if ($stream.Read($header, 0, $header.Length) -ne $header.Length) {
        throw "Fastfile is too short: $output"
    }
}
finally {
    $stream.Dispose()
}

if ([System.Text.Encoding]::ASCII.GetString($header) -ne 'ZW3F') {
    throw "ZoneBuilder produced an unprotected fastfile: $output"
}

if ((Get-Item -LiteralPath $stderr).Length -ne 0) {
    throw "ZoneBuilder wrote diagnostics to stderr: $stderr"
}

if ($Deploy) {
    $destinationDirectory = Join-Path $Root "usermaps\$Map"
    $destination = Join-Path $destinationDirectory "$Map.ff"
    New-Item -ItemType Directory -Path $destinationDirectory -Force | Out-Null

    if (Test-Path -LiteralPath $destination) {
        Copy-Item -LiteralPath $destination -Destination ($destination + ".before_zw3f_$timestamp")
    }

    Copy-Item -LiteralPath $output -Destination $destination -Force
}

[pscustomobject]@{
    Map = $Map
    Output = $output
    Deployed = [bool]$Deploy
    Stdout = $stdout
    Stderr = $stderr
    Length = (Get-Item -LiteralPath $output).Length
    SHA256 = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash
}
