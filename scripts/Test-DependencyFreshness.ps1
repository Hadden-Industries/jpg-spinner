[CmdletBinding()]
param(
    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot),

    [Parameter()]
    [string]$MetadataSnapshotDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$jsonObjectMemberValidationModulePath =
    Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1'
Import-Module -Name $jsonObjectMemberValidationModulePath -Force
$strictJsonWebRequestModulePath = Join-Path $PSScriptRoot 'StrictJsonWebRequest.psm1'
Import-Module -Name $strictJsonWebRequestModulePath -Force

$resolvedRepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$resolvedMetadataSnapshotDirectory = if ([string]::IsNullOrWhiteSpace($MetadataSnapshotDirectory)) {
    $null
}
else {
    [System.IO.Path]::GetFullPath($MetadataSnapshotDirectory)
}

if (-not (Test-Path -LiteralPath $resolvedRepositoryRoot -PathType Container)) {
    throw "Repository root does not exist: $resolvedRepositoryRoot"
}
if ($null -ne $resolvedMetadataSnapshotDirectory -and
    -not (Test-Path -LiteralPath $resolvedMetadataSnapshotDirectory -PathType Container)) {
    throw "Metadata snapshot directory does not exist: $resolvedMetadataSnapshotDirectory"
}

$availableUpdates = [System.Collections.Generic.List[string]]::new()
$releaseChannelNotices = [System.Collections.Generic.List[string]]::new()
$approvedPackageAuthorities =
    [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

function Read-JsonFile {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Description,

        [Parameter(Mandatory)]
        [System.Text.Json.JsonValueKind]$RequiredRootValueKind
    )

    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw "$Description is absent: $Path"
    }

    try {
        $jsonText = Get-Content -LiteralPath $Path -Raw
        return ConvertFrom-JsonWithUniqueObjectMembers `
            -JsonText $jsonText `
            -SourceDescription $Description `
            -RequiredRootValueKind $RequiredRootValueKind `
            -MaximumDepth 100
    }
    catch {
        throw "$Description is not valid JSON: $($_.Exception.Message)"
    }
}

function Read-OfficialJsonMetadata {
    param(
        [Parameter(Mandatory)]
        [uri]$Uri,

        [Parameter(Mandatory)]
        [string]$SnapshotRelativePath,

        [Parameter(Mandatory)]
        [string]$Description,

        [Parameter(Mandatory)]
        [System.Text.Json.JsonValueKind]$RequiredRootValueKind
    )

    if ($null -ne $resolvedMetadataSnapshotDirectory) {
        $snapshotPath = Join-Path $resolvedMetadataSnapshotDirectory $SnapshotRelativePath
        return Read-JsonFile `
            -Path $snapshotPath `
            -Description "$Description snapshot" `
            -RequiredRootValueKind $RequiredRootValueKind
    }

    try {
        # GitHub requires a User-Agent, and the explicit Accept value also keeps
        # response selection stable if its API default changes in the future.
        return Invoke-JsonWebRequestWithUniqueObjectMembers `
            -Uri $Uri `
            -Headers @{
                Accept = 'application/json'
                'User-Agent' = 'JPG-Spinner-Dependency-Freshness/2.0'
            } `
            -SourceDescription $Description `
            -RequiredRootValueKind $RequiredRootValueKind `
            -MaximumDepth 100
    }
    catch {
        throw "Unable to read $Description from '$Uri': $($_.Exception.Message)"
    }
}

function ConvertTo-StableVersionRecord {
    param(
        [Parameter(Mandatory)]
        [string]$CandidateVersion
    )

    $versionWithoutTagPrefix = $CandidateVersion.Trim() -replace '^[vV]', ''

    # vcpkg renders a nonzero packaging revision as `<version>#<port-version>`.
    # Split that suffix before parsing the upstream version so revision #1 sorts
    # after revision #0 even when their upstream components are identical.
    $primaryVersionText = $versionWithoutTagPrefix
    $portVersion = [uint64]0
    if ($primaryVersionText.Contains('#')) {
        $portVersionMatch = [regex]::Match(
            $primaryVersionText,
            '^(?<primaryVersion>.+)#(?<portVersion>0|[1-9]\d*)$'
        )
        if (-not $portVersionMatch.Success) {
            return $null
        }

        $primaryVersionText = $portVersionMatch.Groups['primaryVersion'].Value
        if (-not [uint64]::TryParse(
                $portVersionMatch.Groups['portVersion'].Value,
                [Globalization.NumberStyles]::None,
                [Globalization.CultureInfo]::InvariantCulture,
                [ref]$portVersion
            )) {
            return $null
        }
    }

    if ($primaryVersionText.Contains('-')) {
        # A SemVer hyphen introduces a prerelease. Stable-only freshness checks
        # intentionally ignore previews, release candidates, and nightly tags.
        return $null
    }

    $versionWithoutBuildMetadata = ($primaryVersionText -split '\+', 2)[0]
    if ($versionWithoutBuildMetadata -notmatch '^\d+(?:\.\d+)*$') {
        return $null
    }

    $components = @(
        $versionWithoutBuildMetadata.Split('.') |
            ForEach-Object { [uint64]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) }
    )
    $normalizedVersion = if ($portVersion -eq 0) {
        $versionWithoutBuildMetadata
    }
    else {
        "${versionWithoutBuildMetadata}#$portVersion"
    }

    return [pscustomobject]@{
        originalVersion = $CandidateVersion
        normalizedVersion = $normalizedVersion
        components = $components
        portVersion = $portVersion
    }
}

function Compare-StableVersion {
    param(
        [Parameter(Mandatory)]
        [object]$Left,

        [Parameter(Mandatory)]
        [object]$Right
    )

    $componentCount = [Math]::Max($Left.components.Count, $Right.components.Count)
    for ($componentIndex = 0; $componentIndex -lt $componentCount; $componentIndex++) {
        $leftComponent = if ($componentIndex -lt $Left.components.Count) {
            $Left.components[$componentIndex]
        }
        else {
            [uint64]0
        }
        $rightComponent = if ($componentIndex -lt $Right.components.Count) {
            $Right.components[$componentIndex]
        }
        else {
            [uint64]0
        }

        if ($leftComponent -gt $rightComponent) {
            return 1
        }
        if ($leftComponent -lt $rightComponent) {
            return -1
        }
    }

    if ($Left.portVersion -gt $Right.portVersion) {
        return 1
    }
    if ($Left.portVersion -lt $Right.portVersion) {
        return -1
    }

    return 0
}

function Get-NewestStableVersion {
    param(
        [Parameter(Mandatory)]
        [object[]]$Candidates,

        [Parameter(Mandatory)]
        [string]$Description
    )

    $newestStableVersion = $null
    foreach ($candidate in $Candidates) {
        if ($null -eq $candidate) {
            continue
        }

        $candidateRecord = ConvertTo-StableVersionRecord -CandidateVersion ([string]$candidate)
        if ($null -eq $candidateRecord) {
            continue
        }
        if ($null -eq $newestStableVersion -or
            (Compare-StableVersion -Left $candidateRecord -Right $newestStableVersion) -gt 0) {
            $newestStableVersion = $candidateRecord
        }
    }

    if ($null -eq $newestStableVersion) {
        throw "$Description contains no parseable stable numeric version."
    }

    return $newestStableVersion
}

function Assert-PinnedVersionExistsInAuthority {
    param(
        [Parameter(Mandatory)]
        [string]$DependencyName,

        [Parameter(Mandatory)]
        [string]$PinnedVersion,

        [Parameter(Mandatory)]
        [AllowEmptyCollection()]
        [object[]]$AuthorityVersions,

        [Parameter(Mandatory)]
        [string]$Authority
    )

    $pinnedVersionRecord = ConvertTo-StableVersionRecord -CandidateVersion $PinnedVersion
    if ($null -eq $pinnedVersionRecord) {
        throw "$DependencyName has non-stable or unsupported pin '$PinnedVersion'."
    }

    # Availability is an identity question, not an ordering question. Compare
    # canonical stable representations exactly so a higher typo cannot look
    # current merely because no published version sorts after it. For vcpkg,
    # canonicalization also retains every nonzero #port-version component.
    $publishedNormalizedVersions = @(
        foreach ($authorityVersion in $AuthorityVersions) {
            $publishedVersionRecord =
                ConvertTo-StableVersionRecord -CandidateVersion ([string]$authorityVersion)
            if ($null -ne $publishedVersionRecord) {
                $publishedVersionRecord.normalizedVersion
            }
        }
    )
    if ($pinnedVersionRecord.normalizedVersion -cnotin $publishedNormalizedVersions) {
        throw (
            "$DependencyName pins $($pinnedVersionRecord.normalizedVersion), but $Authority " +
            'does not contain that exact version.'
        )
    }
}

function Add-AvailableUpdate {
    param(
        [Parameter(Mandatory)]
        [string]$DependencyName,

        [Parameter(Mandatory)]
        [string]$PinnedVersion,

        [Parameter(Mandatory)]
        [object]$AvailableVersion,

        [Parameter(Mandatory)]
        [string]$Authority
    )

    $pinnedVersionRecord = ConvertTo-StableVersionRecord -CandidateVersion $PinnedVersion
    if ($null -eq $pinnedVersionRecord) {
        throw "$DependencyName has non-stable or unsupported pin '$PinnedVersion'."
    }
    if ((Compare-StableVersion -Left $AvailableVersion -Right $pinnedVersionRecord) -gt 0) {
        [void]$availableUpdates.Add(
            "${DependencyName}: pinned $($pinnedVersionRecord.normalizedVersion); $Authority offers $($AvailableVersion.normalizedVersion)."
        )
    }
}

$vcpkgManifestPath = Join-Path $resolvedRepositoryRoot 'vcpkg.json'
$vcpkgManifest = Read-JsonFile `
    -Path $vcpkgManifestPath `
    -Description 'vcpkg manifest' `
    -RequiredRootValueKind Object
$vcpkgOverrides = @($vcpkgManifest.overrides)
if ($vcpkgOverrides.Count -eq 0) {
    throw 'vcpkg.json must contain exact overrides before dependency freshness can be evaluated.'
}

$upstreamGitHubRepositories = @{
    'libjpeg-turbo' = 'libjpeg-turbo/libjpeg-turbo'
    exiv2 = 'Exiv2/exiv2'
    catch2 = 'catchorg/Catch2'
}

# Overlay ports take precedence over registry version selection. Their local
# packaging revision is not an official registry revision with the same number.
# Recognize only the approved pair; repository policy validates the full build
# configuration, while this read-only check verifies the selected version source.
$usesMetadataOverlays = $false
$configurationProperty = $vcpkgManifest.PSObject.Properties['configuration']
$resolutionConfiguration = if ($null -ne $configurationProperty) { $configurationProperty.Value } else { $null }
$standaloneConfigurationPath = Join-Path $resolvedRepositoryRoot 'vcpkg-configuration.json'
if (Test-Path -LiteralPath $standaloneConfigurationPath -PathType Leaf) {
    if ($null -ne $configurationProperty) {
        throw "vcpkg-configuration.json cannot coexist with vcpkg.json embedded 'configuration'."
    }
    # Use the same strict raw-JSON boundary for both native representations.
    $resolutionConfiguration = Read-JsonFile -Path $standaloneConfigurationPath `
        -Description 'standalone vcpkg configuration' -RequiredRootValueKind Object
}
if ($null -ne $resolutionConfiguration) {
    $overlayProperty = $resolutionConfiguration.PSObject.Properties['overlay-ports']
    if ($null -ne $overlayProperty) {
        $paths = $overlayProperty.Value
        if ($paths -isnot [array] -or $paths.Count -ne 2 -or
            $paths[0] -cne 'vcpkg-ports/adobe-xmp-core' -or
            $paths[1] -cne 'vcpkg-ports/exiv2') {
            throw 'Dependency freshness supports only the approved metadata overlay paths.'
        }
        $usesMetadataOverlays = $true
    }
}

if ($usesMetadataOverlays) {
    $localExivManifest = Read-JsonFile `
        -Path (Join-Path $resolvedRepositoryRoot 'vcpkg-ports/exiv2/vcpkg.json') `
        -Description 'local Exiv2 overlay manifest' -RequiredRootValueKind Object
    $adobeManifest = Read-JsonFile `
        -Path (Join-Path $resolvedRepositoryRoot 'vcpkg-ports/adobe-xmp-core/vcpkg.json') `
        -Description 'local Adobe XMP overlay manifest' -RequiredRootValueKind Object
    if ($adobeManifest.name -cne 'adobe-xmp-core') {
        throw 'The approved Adobe overlay must declare adobe-xmp-core.'
    }
    $adobeReleases = Read-OfficialJsonMetadata `
        -Uri 'https://api.github.com/repos/adobe/XMP-Toolkit-SDK/releases?per_page=100' `
        -SnapshotRelativePath 'github/adobe-xmp-core.releases.json' `
        -Description 'upstream Adobe XMP Toolkit releases' -RequiredRootValueKind Array
    $adobeStableTags = @($adobeReleases | Where-Object {
        $_.draft -eq $false -and $_.prerelease -eq $false
    } | ForEach-Object { $_.tag_name })
    Assert-PinnedVersionExistsInAuthority -DependencyName 'adobe-xmp-core' `
        -PinnedVersion $adobeManifest.'version-string' `
        -AuthorityVersions $adobeStableTags -Authority 'upstream Adobe XMP Toolkit'
    Add-AvailableUpdate -DependencyName 'adobe-xmp-core' `
        -PinnedVersion $adobeManifest.'version-string' `
        -AvailableVersion (Get-NewestStableVersion -Candidates $adobeStableTags -Description 'Adobe XMP releases') `
        -Authority 'upstream Adobe XMP Toolkit'
    [void]$approvedPackageAuthorities.Add('approved local metadata overlays and upstream Adobe XMP Toolkit')
}

foreach ($override in $vcpkgOverrides) {
    $dependencyName = [string]$override.name
    $pinnedVersion = [string]$override.version
    if ([string]::IsNullOrWhiteSpace($dependencyName) -or [string]::IsNullOrWhiteSpace($pinnedVersion)) {
        throw 'Every vcpkg override must contain a dependency name and exact version.'
    }

    $isLocalExivOverlay = $usesMetadataOverlays -and $dependencyName -ceq 'exiv2'
    if ($isLocalExivOverlay) {
        if ($localExivManifest.name -cne 'exiv2' -or
            $pinnedVersion -cne "$($localExivManifest.version)#$($localExivManifest.'port-version')") {
            throw 'The local exiv2 overlay does not match the exact root override.'
        }
        # Compare upstream source versions below, never unrelated revision
        # counters maintained by two different package authorities.
        $pinnedVersion = [string]$localExivManifest.version
    }

    $registryBucket = "$($dependencyName.Substring(0, 1).ToLowerInvariant())-"
    $registryMetadata = Read-OfficialJsonMetadata `
        -Uri "https://raw.githubusercontent.com/microsoft/vcpkg/master/versions/$registryBucket/$dependencyName.json" `
        -SnapshotRelativePath "vcpkg/$dependencyName.json" `
        -Description "official vcpkg registry metadata for $dependencyName" `
        -RequiredRootValueKind Object
    [void]$approvedPackageAuthorities.Add('official vcpkg registry')

    $registryVersions = @(
        foreach ($registryVersion in @($registryMetadata.versions)) {
            $declaredVersion = $null
            foreach ($versionPropertyName in @('version-semver', 'version', 'version-string', 'version-date')) {
                $versionProperty = $registryVersion.PSObject.Properties[$versionPropertyName]
                if ($null -ne $versionProperty) {
                    $declaredVersion = [string]$versionProperty.Value
                    break
                }
            }

            if ([string]::IsNullOrWhiteSpace($declaredVersion)) {
                continue
            }

            $registryPortVersion = [uint64]0
            $portVersionProperty = $registryVersion.PSObject.Properties['port-version']
            if ($null -ne $portVersionProperty) {
                $portVersionText = [string]$portVersionProperty.Value
                if (
                    $portVersionText -notmatch '^(?:0|[1-9]\d*)$' -or
                    -not [uint64]::TryParse(
                        $portVersionText,
                        [Globalization.NumberStyles]::None,
                        [Globalization.CultureInfo]::InvariantCulture,
                        [ref]$registryPortVersion
                    )
                ) {
                    throw (
                        "Official vcpkg registry metadata for $dependencyName contains invalid " +
                        "port-version '$portVersionText'; expected a non-negative integer."
                    )
                }
            }

            if ($registryPortVersion -eq 0) {
                $declaredVersion
            }
            else {
                "${declaredVersion}#$registryPortVersion"
            }
        }
    )
    if ($isLocalExivOverlay) {
        $registryVersions = @($registryVersions | ForEach-Object { ($_ -split '#')[0] })
    }
    Assert-PinnedVersionExistsInAuthority `
        -DependencyName $dependencyName `
        -PinnedVersion $pinnedVersion `
        -AuthorityVersions $registryVersions `
        -Authority 'official vcpkg registry'
    $newestRegistryVersion = Get-NewestStableVersion `
        -Candidates $registryVersions `
        -Description "official vcpkg registry metadata for $dependencyName"
    Add-AvailableUpdate `
        -DependencyName $dependencyName `
        -PinnedVersion $pinnedVersion `
        -AvailableVersion $newestRegistryVersion `
        -Authority 'official vcpkg registry'

    if ($upstreamGitHubRepositories.ContainsKey($dependencyName)) {
        $githubRepository = $upstreamGitHubRepositories[$dependencyName]
        $githubMetadata = Read-OfficialJsonMetadata `
            -Uri "https://api.github.com/repos/$githubRepository/releases?per_page=100" `
            -SnapshotRelativePath "github/$dependencyName.releases.json" `
            -Description "upstream GitHub releases for $dependencyName" `
            -RequiredRootValueKind Array
        $stableReleaseTags = @(
            foreach ($release in @($githubMetadata)) {
                if ($release.draft -eq $true -or $release.prerelease -eq $true) {
                    continue
                }
                [string]$release.tag_name
            }
        )
        $newestGitHubVersion = Get-NewestStableVersion `
            -Candidates $stableReleaseTags `
            -Description "upstream GitHub releases for $dependencyName"
        $pinnedVersionRecord = ConvertTo-StableVersionRecord -CandidateVersion $pinnedVersion
        if ((Compare-StableVersion -Left $newestGitHubVersion -Right $pinnedVersionRecord) -gt 0) {
            if ($isLocalExivOverlay -or
                (Compare-StableVersion -Left $newestRegistryVersion -Right $newestGitHubVersion) -ge 0) {
                Add-AvailableUpdate `
                    -DependencyName $dependencyName `
                    -PinnedVersion $pinnedVersion `
                    -AvailableVersion $newestGitHubVersion `
                    -Authority 'upstream GitHub'
            }
            else {
                [void]$releaseChannelNotices.Add(
                    "${dependencyName}: upstream GitHub offers stable $($newestGitHubVersion.normalizedVersion), " +
                    "but it is not yet present in the official vcpkg registry (latest $($newestRegistryVersion.normalizedVersion)); " +
                    'the manifest remains on the newest version that the selected official registry can resolve.'
                )
            }
        }
    }
}

$centralPackageVersionsPath = Join-Path $resolvedRepositoryRoot 'Directory.Packages.props'
if (Test-Path -LiteralPath $centralPackageVersionsPath -PathType Leaf) {
    try {
        $centralPackageVersions = [System.Xml.XmlDocument]::new()
        $centralPackageVersions.Load($centralPackageVersionsPath)
    }
    catch {
        throw "Directory.Packages.props is not valid XML: $($_.Exception.Message)"
    }

    $packageVersionNodes = @($centralPackageVersions.SelectNodes('/Project/ItemGroup/PackageVersion'))
    if ($packageVersionNodes.Count -eq 0) {
        throw 'Directory.Packages.props contains no PackageVersion items.'
    }
    foreach ($packageVersionNode in $packageVersionNodes) {
        $packageId = $packageVersionNode.GetAttribute('Include')
        $pinnedVersion = $packageVersionNode.GetAttribute('Version')
        if ([string]::IsNullOrWhiteSpace($packageId) -or [string]::IsNullOrWhiteSpace($pinnedVersion)) {
            throw 'Every PackageVersion must contain Include and exact Version attributes.'
        }

        $lowerPackageId = $packageId.ToLowerInvariant()
        $nuGetMetadata = Read-OfficialJsonMetadata `
            -Uri "https://api.nuget.org/v3-flatcontainer/$lowerPackageId/index.json" `
            -SnapshotRelativePath "nuget/$lowerPackageId.index.json" `
            -Description "official NuGet metadata for $packageId" `
            -RequiredRootValueKind Object
        [void]$approvedPackageAuthorities.Add('official NuGet v3 metadata')

        Assert-PinnedVersionExistsInAuthority `
            -DependencyName $packageId `
            -PinnedVersion $pinnedVersion `
            -AuthorityVersions @($nuGetMetadata.versions) `
            -Authority 'official NuGet'
        $newestNuGetVersion = Get-NewestStableVersion `
            -Candidates @($nuGetMetadata.versions) `
            -Description "official NuGet metadata for $packageId"
        Add-AvailableUpdate `
            -DependencyName $packageId `
            -PinnedVersion $pinnedVersion `
            -AvailableVersion $newestNuGetVersion `
            -Authority 'official NuGet'
    }
}

if ($availableUpdates.Count -gt 0) {
    # Write the report directly to stderr so CI receives stable, unwrapped text
    # rather than PowerShell's host-formatted ErrorRecord with source excerpts.
    [Console]::Error.WriteLine(
        "Newer stable dependency versions are available; manifests were not changed:`n - $($availableUpdates -join "`n - ")"
    )
    exit 1
}

$releaseChannelNotices | ForEach-Object { Write-Output "Dependency release-channel notice: $_" }
$packageAuthoritySummary = @($approvedPackageAuthorities | Sort-Object) -join ', '
Write-Output (
    'No newer dependency versions actionable through the approved package authorities were found. ' +
    "Checked $packageAuthoritySummary. " +
    'Upstream-only release-channel lag, if any, is reported above. ' +
    'Repository manifests were not changed.'
)
