[CmdletBinding()]
param(
    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$resolvedRepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $resolvedRepositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $resolvedRepositoryRoot"
}

$toolchainLockPath = Join-Path $resolvedRepositoryRoot 'eng/toolchain-lock.json'
if (Test-Path -LiteralPath $toolchainLockPath) {
    throw "Toolchain lock already exists and will not be replaced: $toolchainLockPath"
}

$resolverPath = Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1'
$resolvedToolchain = & $resolverPath -RepositoryRoot $resolvedRepositoryRoot

$lockDirectory = Split-Path -Parent $toolchainLockPath
[void](New-Item -ItemType Directory -Path $lockDirectory -Force)

$toolchainLock = [ordered]@{
    schemaVersion = 1
    visualStudioMajorVersion = $resolvedToolchain.visualStudioMajorVersion
    platformToolset = $resolvedToolchain.platformToolset
    vcToolsVersion = $resolvedToolchain.vcToolsVersion
}

# CreateNew gives the refusal-to-overwrite rule an operating-system-enforced
# race boundary instead of relying only on the earlier existence check.
$utf8WithoutByteOrderMark = [System.Text.UTF8Encoding]::new($false)
$jsonBytes = $utf8WithoutByteOrderMark.GetBytes(($toolchainLock | ConvertTo-Json -Depth 10) + "`n")
$lockStream = [System.IO.FileStream]::new(
    $toolchainLockPath,
    [System.IO.FileMode]::CreateNew,
    [System.IO.FileAccess]::Write,
    [System.IO.FileShare]::None
)
try {
    $lockStream.Write($jsonBytes, 0, $jsonBytes.Length)
    $lockStream.Flush($true)
}
finally {
    $lockStream.Dispose()
}

[pscustomobject]$toolchainLock
