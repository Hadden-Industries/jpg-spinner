<# Compose caller-frozen native export snapshots and an application NuGet lock.
   Native vcpkg already selected each installed closure; NuGet already resolved
   every lock target. This script copies their original bytes and enumerates
   resolved identities, never traverses dependency edges or restores packages.
   Membership is build context, not static linkage or runtime qualification.
   https://learn.microsoft.com/nuget/consume-packages/package-references-in-project-files#locking-dependencies #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][hashtable]$NativeEvidenceDirectories,
    [Parameter(Mandatory)][string]$NuGetLockPath,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$WorkingDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$architectures=@('arm64','x64','x86')
if ($NativeEvidenceDirectories.Count -ne 3 -or
    @($NativeEvidenceDirectories.Keys | Where-Object {$_ -cnotin $architectures}).Count -ne 0) {
    throw 'Build-component evidence requires exactly arm64, x64 and x86 native snapshots.'
}
$output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
$working=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
$lockPath=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($NuGetLockPath)
$native=@{}
foreach ($architecture in $architectures) {
    $native[$architecture]=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($NativeEvidenceDirectories[$architecture])
    if (-not [IO.Directory]::Exists($native[$architecture])) {throw 'A native component snapshot is absent.'}
}
if (-not [IO.File]::Exists($lockPath)) {throw 'The resolved application NuGet lock is absent.'}
if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $working)) {throw 'Build-component destinations must be fresh and absent.'}
foreach ($path in @($lockPath,$output,$working)+@($native.Values)) {
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and
            ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'Build-component composition refuses reparse-point inputs or output ancestors.'
        }
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
}
function Test-PathWithin([string]$Root,[string]$Path) {
    $relative=[IO.Path]::GetRelativePath($Root,$Path)
    return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
        -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)
}
foreach ($destination in @($working,$output)) {
    if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) {throw 'Build-component destination parents must exist.'}
    if (Test-PathWithin $destination $lockPath) {throw 'Build-component destinations overlap inputs.'}
    foreach ($inputRoot in $native.Values) {
        if ((Test-PathWithin $inputRoot $destination) -or (Test-PathWithin $destination $inputRoot)) {throw 'Build-component destinations overlap inputs.'}
    }
}
if ((Test-PathWithin $working $output) -or (Test-PathWithin $output $working) -or
    -not [string]::Equals([IO.Path]::GetPathRoot($working),[IO.Path]::GetPathRoot($output),[StringComparison]::OrdinalIgnoreCase)) {
    throw 'Build-component destinations must be disjoint and on the same volume.'
}
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$lockHandle=[IO.File]::OpenRead($lockPath)
try {
    $lock=ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText($lockPath)) `
        -RequiredRootValueKind Object -SourceDescription 'Resolved application NuGet lock' -AsHashtable
    if (-not $lock.Contains('version') -or -not $lock.Contains('dependencies') -or
        $lock.version -isnot [long] -or $lock.version -ne 2 -or
        $lock.dependencies -isnot [Collections.IDictionary] -or $lock.dependencies.Count -eq 0) {
        throw 'Resolved application NuGet lock has an unsupported shape.'
    }
    $required=[Collections.Generic.List[object]]::new()
    $nugetIdentities=[Collections.Generic.Dictionary[string,object]]::new([StringComparer]::Ordinal)
    foreach ($target in $lock.dependencies.Values) {
        if ($target -isnot [Collections.IDictionary]) {throw 'NuGet lock target is not an object.'}
        foreach ($entry in $target.GetEnumerator()) {
            if ($entry.Value -isnot [Collections.IDictionary] -or -not $entry.Value.Contains('type')) {throw 'NuGet lock dependency has an unsupported shape.'}
            # A Project entry represents owned source, not a resolved external
            # package. Preserve it in the original lock but not requirements.
            if ($entry.Value.type -ceq 'Project') {continue}
            if (-not $entry.Value.Contains('resolved') -or -not $entry.Value.Contains('contentHash') -or
                [string]::IsNullOrEmpty($entry.Key) -or $entry.Value.type -cnotin @('Direct','Transitive','CentralTransitive') -or
                $entry.Value.resolved -isnot [string] -or [string]::IsNullOrEmpty($entry.Value.resolved) -or
                $entry.Value.contentHash -isnot [string] -or [string]::IsNullOrEmpty($entry.Value.contentHash)) {
                throw 'NuGet lock dependency lacks a supported resolved package identity.'
            }
            # NuGet package IDs are case-insensitive; versions and content hashes
            # remain exact vendor data. Refuse inconsistent spellings/content
            # rather than choosing a different identity than the native cataloger.
            $key=ConvertTo-Json -InputObject @($entry.Key.ToLowerInvariant(),$entry.Value.resolved) -Compress
            if ($nugetIdentities.ContainsKey($key)) {
                $previous=$nugetIdentities[$key]
                if ($previous.name -cne $entry.Key -or $previous.hash -cne $entry.Value.contentHash) {throw 'Repeated NuGet package identity disagrees across lock targets.'}
                continue
            }
            $nugetIdentities.Add($key,@{name=$entry.Key;hash=$entry.Value.contentHash})
            $required.Add(@{name=$entry.Key;version=$entry.Value.resolved})
        }
    }
    if ($nugetIdentities.Count -eq 0) {throw 'NuGet lock contains no resolved external packages.'}
    # Snapshot before copying; the reused inventory producer rejects descendant
    # links and supplies ordinal relative paths and independent SHA-256 digests.
    [void][IO.Directory]::CreateDirectory($working)
    $candidate=Join-Path $working 'candidate'
    $components=Join-Path $candidate 'components'
    [void][IO.Directory]::CreateDirectory($components)
    foreach ($architecture in $architectures) {
        $source=$native[$architecture]
        $inventory=Join-Path $working "$architecture-before.json"
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $source -OutputPath $inventory | Out-Null
        $files=(ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText($inventory)) -RequiredRootValueKind Object -SourceDescription 'Independent native component inventory' -AsHashtable).files
        $paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($file in $files) {[void]$paths.Add($file.relativePath)}
        $contract=ConvertFrom-JsonWithUniqueObjectMembers -JsonText ([IO.File]::ReadAllText((Join-Path $source 'required-components.json'))) `
            -RequiredRootValueKind Object -SourceDescription 'Native exported component contract' -AsHashtable
        if ($contract.requiredPackages -isnot [array] -or $contract.requiredPackages.Count -eq 0) {throw 'Native export requirements are empty.'}
        $names=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
        foreach ($identity in $contract.requiredPackages) {
            if ($identity -isnot [Collections.IDictionary] -or $identity.Keys.Count -ne 3 -or
                -not $identity.Contains('name') -or -not $identity.Contains('version') -or -not $identity.Contains('packageUrl') -or
                $identity.name -isnot [string] -or [string]::IsNullOrEmpty($identity.name) -or
                $identity.version -isnot [string] -or [string]::IsNullOrEmpty($identity.version) -or
                $identity.packageUrl -isnot [string] -or [string]::IsNullOrEmpty($identity.packageUrl) -or
                -not $names.Add($identity.name) -or
                -not $paths.Contains("installed/$architecture-windows-static-md/share/$($identity.name)/vcpkg.spdx.json")) {
                throw 'Native export requirements differ from their architecture-specific metadata snapshot.'
            }
            $required.Add($identity)
        }
        if (-not $names.Contains('exiv2') -or -not $names.Contains('libjpeg-turbo') -or $paths.Count -ne $names.Count+1) {
            throw 'Native export snapshot does not contain exactly its declared closure and application roots.'
        }
        $copy=Join-Path $components "native/$architecture"
        [void][IO.Directory]::CreateDirectory($copy)
        foreach ($file in $files) {
            $destination=Join-Path $copy $file.relativePath
            [void][IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($destination))
            [IO.File]::Copy((Join-Path $source $file.relativePath),$destination,$false)
        }
        $copiedInventory=Join-Path $working "$architecture-copy.json"
        $after=Join-Path $working "$architecture-after.json"
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $copy -OutputPath $copiedInventory | Out-Null
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $source -OutputPath $after | Out-Null
        $beforeHash=(Get-FileHash -LiteralPath $inventory).Hash
        if ($beforeHash -cne (Get-FileHash -LiteralPath $copiedInventory).Hash -or $beforeHash -cne (Get-FileHash -LiteralPath $after).Hash) {
            throw 'Native component snapshot changed during composition.'
        }
    }
    [IO.File]::Copy($lockPath,(Join-Path $components 'packages.lock.json'),$false)
    $contractPath=Join-Path $candidate 'required-components.json'
    @{requiredPackages=@($required)} | ConvertTo-Json -Depth 8 | Out-File -LiteralPath $contractPath -Encoding utf8NoBOM -NoClobber
    [IO.Directory]::Move($candidate,$output)
}
finally {$lockHandle.Dispose()}
[pscustomobject]@{ComponentEvidenceRoot=(Join-Path $output 'components');ComponentContractPath=(Join-Path $output 'required-components.json')}
