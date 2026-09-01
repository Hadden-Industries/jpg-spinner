[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$lockCreatorPath = Join-Path $repositoryRoot 'scripts/New-MSBuildToolchainLock.ps1'
if (-not (Test-Path -LiteralPath $lockCreatorPath -PathType Leaf)) {
    throw "Expected lock creator is absent: scripts/New-MSBuildToolchainLock.ps1"
}

$temporaryParent = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$temporaryRepositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $temporaryParent "jpg-spinner-toolchain-lock-$([guid]::NewGuid().ToString('N'))")
)
if (-not $temporaryRepositoryRoot.StartsWith($temporaryParent, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to create a test repository outside the operating-system temporary directory: $temporaryRepositoryRoot"
}

try {
    [void](New-Item -ItemType Directory -Path $temporaryRepositoryRoot)

    $createdLock = & $lockCreatorPath -RepositoryRoot $temporaryRepositoryRoot
    $lockPath = Join-Path $temporaryRepositoryRoot 'eng/toolchain-lock.json'
    if (-not (Test-Path -LiteralPath $lockPath -PathType Leaf)) {
        throw "The lock creator did not create the expected file: $lockPath"
    }

    $persistedLock = Get-Content -LiteralPath $lockPath -Raw | ConvertFrom-Json -AsHashtable
    if ($persistedLock.schemaVersion -ne 1) {
        throw "Expected lock schemaVersion 1; found '$($persistedLock.schemaVersion)'."
    }
    if ([string]$persistedLock.vcToolsVersion -cne [string]$createdLock.vcToolsVersion) {
        throw 'The returned toolchain record and persisted lock disagree on vcToolsVersion.'
    }
    if ($persistedLock.ContainsKey('visualStudioInstallationVersion')) {
        throw 'The lock must not imply that an informational Visual Studio product build is enforced.'
    }

    $secondCreationWasRejected = $false
    try {
        & $lockCreatorPath -RepositoryRoot $temporaryRepositoryRoot | Out-Null
    }
    catch {
        $secondCreationWasRejected = $_.Exception.Message -match 'already exists'
    }
    if (-not $secondCreationWasRejected) {
        throw 'The lock creator must reject an existing lock instead of replacing it.'
    }

    $lockAfterRejectedCreation = Get-Content -LiteralPath $lockPath -Raw
    $lockBeforeRejectedCreation = $persistedLock | ConvertTo-Json -Depth 10
    $normalizedPersistedLock = $lockAfterRejectedCreation | ConvertFrom-Json -AsHashtable | ConvertTo-Json -Depth 10
    if ($normalizedPersistedLock -cne $lockBeforeRejectedCreation) {
        throw 'A rejected second creation attempt changed the existing toolchain lock.'
    }

    Write-Output "PASS: created immutable MSVC $($persistedLock.vcToolsVersion) toolchain lock and rejected replacement."
}
finally {
    # The exact absolute target was constructed beneath the OS temp directory
    # and validated above before this recursive cleanup is allowed to run.
    if (Test-Path -LiteralPath $temporaryRepositoryRoot) {
        Remove-Item -LiteralPath $temporaryRepositoryRoot -Recurse -Force
    }
}
