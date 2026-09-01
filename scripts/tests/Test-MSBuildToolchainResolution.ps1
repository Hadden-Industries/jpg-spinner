[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$resolverPath = Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1'

if (-not (Test-Path -LiteralPath $resolverPath -PathType Leaf)) {
    throw "Expected read-only resolver is absent: scripts/Resolve-MSBuildToolchain.ps1"
}

$resolvedToolchain = & $resolverPath -RepositoryRoot $repositoryRoot
if ($null -eq $resolvedToolchain) {
    throw 'Resolve-MSBuildToolchain.ps1 returned no toolchain record.'
}

$expectedPathProperties = @(
    'visualStudioInstallationPath',
    'msBuildExecutablePath',
    'msBuildAssemblyPath',
    'cmakeExecutablePath',
    'vcToolsDirectoryPath'
)
foreach ($propertyName in $expectedPathProperties) {
    $path = [string]$resolvedToolchain.$propertyName
    if (-not [System.IO.Path]::IsPathFullyQualified($path)) {
        throw "$propertyName must be an absolute path; found '$path'."
    }
    if (-not (Test-Path -LiteralPath $path)) {
        throw "$propertyName does not exist: $path"
    }
}

foreach ($targetArchitecture in @('x86', 'x64', 'arm64')) {
    $targetTools = $resolvedToolchain.targetTools[$targetArchitecture]
    if ($null -eq $targetTools) {
        throw "Resolved toolchain lacks target tools for $targetArchitecture."
    }
    if ([string]$targetTools.targetArchitecture -cne $targetArchitecture) {
        throw "Target-tool record key '$targetArchitecture' disagrees with targetArchitecture '$($targetTools.targetArchitecture)'."
    }

    foreach ($propertyName in @('compilerExecutablePath', 'linkerExecutablePath')) {
        $path = [string]$targetTools.$propertyName
        if (-not [System.IO.Path]::IsPathFullyQualified($path)) {
            throw "$targetArchitecture $propertyName must be an absolute path; found '$path'."
        }
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "$targetArchitecture $propertyName does not exist: $path"
        }
    }
}

if ($resolvedToolchain.visualStudioMajorVersion -ne 18) {
    throw "Expected Visual Studio major version 18; found '$($resolvedToolchain.visualStudioMajorVersion)'."
}
if ([string]$resolvedToolchain.visualStudioChannelId -cne 'VisualStudio.18.Release') {
    throw "Expected the stable Visual Studio 18 release channel; found '$($resolvedToolchain.visualStudioChannelId)'."
}
if ($resolvedToolchain.isPrerelease -ne $false) {
    throw 'A prerelease Visual Studio instance must never satisfy toolchain resolution.'
}
if ([string]$resolvedToolchain.platformToolset -cne 'v145') {
    throw "Expected PlatformToolset v145; found '$($resolvedToolchain.platformToolset)'."
}
if ([string]$resolvedToolchain.vcToolsVersion -notmatch '^14\.51\.\d+$') {
    throw "Expected an exact MSVC 14.51 servicing directory; found '$($resolvedToolchain.vcToolsVersion)'."
}

# A lock is a supply-chain authority, so duplicate members must not acquire
# last-member-wins semantics from ConvertFrom-Json. Keep the installed version
# last to prove the resolver rejects ambiguity rather than merely rejecting an
# unavailable effective value.
$temporaryParent = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
$duplicateLockRepositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $temporaryParent "jpg-spinner-duplicate-toolchain-lock-$([guid]::NewGuid().ToString('N'))")
)
if (-not $duplicateLockRepositoryRoot.StartsWith(
        $temporaryParent,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
    throw (
        'Refusing to create a resolver fixture outside the operating-system ' +
        "temporary directory: $duplicateLockRepositoryRoot"
    )
}

try {
    $duplicateLockDirectory = Join-Path $duplicateLockRepositoryRoot 'eng'
    [void][System.IO.Directory]::CreateDirectory($duplicateLockDirectory)
    $duplicateLockContent = @"
{
  "schemaVersion": 1,
  "visualStudioMajorVersion": 18,
  "platformToolset": "v145",
  "vcToolsVersion": "14.51.00000",
  "vcToolsVersion": "$($resolvedToolchain.vcToolsVersion)"
}
"@
    [System.IO.File]::WriteAllText(
        (Join-Path $duplicateLockDirectory 'toolchain-lock.json'),
        $duplicateLockContent.Replace("`r`n", "`n") + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )

    $duplicateLockWasRejected = $false
    try {
        & $resolverPath -RepositoryRoot $duplicateLockRepositoryRoot | Out-Null
    }
    catch {
        $duplicateLockWasRejected =
            $_.Exception.Message.IndexOf(
                "Duplicate property 'vcToolsVersion'",
                [System.StringComparison]::Ordinal
            ) -ge 0
    }
    if (-not $duplicateLockWasRejected) {
        throw 'Resolve-MSBuildToolchain.ps1 accepted a duplicate vcToolsVersion member.'
    }

    [System.IO.File]::WriteAllText(
        (Join-Path $duplicateLockDirectory 'toolchain-lock.json'),
        "null`n",
        [System.Text.UTF8Encoding]::new($false)
    )
    $nonObjectLockWasRejectedAtRawJsonBoundary = $false
    try {
        & $resolverPath -RepositoryRoot $duplicateLockRepositoryRoot | Out-Null
    }
    catch {
        $nonObjectLockWasRejectedAtRawJsonBoundary =
            $_.Exception.Message.IndexOf(
                'eng/toolchain-lock.json root must be a JSON object',
                [System.StringComparison]::Ordinal
            ) -ge 0
    }
    if (-not $nonObjectLockWasRejectedAtRawJsonBoundary) {
        throw 'Resolve-MSBuildToolchain.ps1 did not reject a non-object lock at the raw JSON boundary.'
    }
}
finally {
    if (Test-Path -LiteralPath $duplicateLockRepositoryRoot) {
        Remove-Item -LiteralPath $duplicateLockRepositoryRoot -Recurse -Force
    }
}

Write-Output (
    "PASS: resolved stable Visual Studio $($resolvedToolchain.visualStudioInstallationVersion) " +
    "with MSVC $($resolvedToolchain.vcToolsVersion) and rejected ambiguous lock JSON."
)
