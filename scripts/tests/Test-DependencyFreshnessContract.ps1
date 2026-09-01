[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$freshnessScriptPath = Join-Path $repositoryRoot 'scripts/Test-DependencyFreshness.ps1'
$temporaryRoot = Join-Path ([System.IO.Path]::GetTempPath()) "jpg-spinner-dependency-freshness-$([guid]::NewGuid().ToString('N'))"
$fixtureRepositoryRoot = Join-Path $temporaryRoot 'repository'
$metadataSnapshotDirectory = Join-Path $temporaryRoot 'official-metadata'

function Write-Utf8TextFile {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Content
    )

    [void][System.IO.Directory]::CreateDirectory((Split-Path -Parent $Path))
    [System.IO.File]::WriteAllText(
        $Path,
        $Content.Replace("`r`n", "`n") + "`n",
        [System.Text.UTF8Encoding]::new($false)
    )
}

function Write-JsonFixture {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [object]$Value
    )

    Write-Utf8TextFile -Path $Path -Content ($Value | ConvertTo-Json -Depth 20)
}

function Invoke-FreshnessCheck {
    $powerShellExecutablePath = (Get-Process -Id $PID).Path
    $output = & $powerShellExecutablePath `
        -NoProfile `
        -File $freshnessScriptPath `
        -RepositoryRoot $fixtureRepositoryRoot `
        -MetadataSnapshotDirectory $metadataSnapshotDirectory 2>&1

    return [pscustomobject]@{
        exitCode = $LASTEXITCODE
        output = $output -join "`n"
    }
}

function Assert-FileHashUnchanged {
    param(
        [Parameter(Mandatory)]
        [hashtable]$ExpectedHashes
    )

    foreach ($path in $ExpectedHashes.Keys) {
        $actualHash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
        if ($actualHash -cne $ExpectedHashes[$path]) {
            throw "Freshness verification mutated repository input '$path'."
        }
    }
}

function Assert-RejectedMetadataRoot {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$InvalidJson,

        [Parameter(Mandatory)]
        [string]$ExpectedDiagnostic
    )

    $originalContent = [System.IO.File]::ReadAllText($Path)
    try {
        Write-Utf8TextFile -Path $Path -Content $InvalidJson
        $result = Invoke-FreshnessCheck
        if ($result.exitCode -eq 0) {
            throw "Dependency freshness accepted metadata with the wrong JSON root: $Path"
        }
        $normalizedOutput = $result.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
        if ($normalizedOutput.IndexOf(
                $ExpectedDiagnostic,
                [System.StringComparison]::Ordinal
            ) -lt 0) {
            throw (
                "Dependency freshness rejected the wrong metadata root without '$ExpectedDiagnostic'." +
                "`n$($result.output)"
            )
        }
    }
    finally {
        [System.IO.File]::WriteAllText(
            $Path,
            $originalContent,
            [System.Text.UTF8Encoding]::new($false)
        )
    }
}

try {
    [void][System.IO.Directory]::CreateDirectory($fixtureRepositoryRoot)
    [void][System.IO.Directory]::CreateDirectory($metadataSnapshotDirectory)

    Write-JsonFixture -Path (Join-Path $fixtureRepositoryRoot 'vcpkg.json') -Value ([ordered]@{
        name = 'jpg-spinner'
        'version-string' = '2.0.0'
        'builtin-baseline' = '118bba14b94bc040c098c0c15e63c142148c05ca'
        dependencies = @(
            [ordered]@{ name = 'libjpeg-turbo'; 'default-features' = $false },
            [ordered]@{ name = 'exiv2'; 'default-features' = $false; features = @('xmp') },
            [ordered]@{ name = 'catch2'; 'default-features' = $false }
        )
        overrides = @(
            [ordered]@{ name = 'libjpeg-turbo'; version = '3.2.0' },
            [ordered]@{ name = 'exiv2'; version = '0.28.8' },
            [ordered]@{ name = 'catch2'; version = '3.16.0' }
        )
    })
    Write-Utf8TextFile -Path (Join-Path $fixtureRepositoryRoot 'Directory.Packages.props') -Content @'
<Project>
  <ItemGroup>
    <PackageVersion Include="Microsoft.WindowsAppSDK" Version="2.4.0" />
    <PackageVersion Include="Microsoft.Windows.CppWinRT" Version="3.0.260818.1" />
    <PackageVersion Include="Microsoft.Windows.SDK.BuildTools" Version="10.0.28000.2705" />
  </ItemGroup>
</Project>
'@

    $vcpkgVersions = [ordered]@{
        'libjpeg-turbo' = '3.2.0'
        exiv2 = '0.28.8'
        catch2 = '3.16.0'
    }
    foreach ($dependencyName in $vcpkgVersions.Keys) {
        Write-JsonFixture -Path (Join-Path $metadataSnapshotDirectory "vcpkg/$dependencyName.json") -Value ([ordered]@{
            versions = @(
                [ordered]@{
                    'version-semver' = $vcpkgVersions[$dependencyName]
                    'port-version' = 0
                    'git-tree' = '0123456789abcdef0123456789abcdef01234567'
                }
            )
        })
    }

    $githubRepositories = [ordered]@{
        'libjpeg-turbo' = '3.2.0'
        exiv2 = '0.28.8'
        catch2 = '3.16.0'
    }
    foreach ($repositoryName in $githubRepositories.Keys) {
        Write-JsonFixture -Path (Join-Path $metadataSnapshotDirectory "github/$repositoryName.releases.json") -Value @(
            [ordered]@{ tag_name = "v$($githubRepositories[$repositoryName])"; draft = $false; prerelease = $false },
            [ordered]@{ tag_name = 'v99.0.0-preview.1'; draft = $false; prerelease = $true },
            [ordered]@{ tag_name = 'v100.0.0'; draft = $true; prerelease = $false }
        )
    }

    $nuGetVersions = [ordered]@{
        'microsoft.windowsappsdk' = '2.4.0'
        'microsoft.windows.cppwinrt' = '3.0.260818.1'
        'microsoft.windows.sdk.buildtools' = '10.0.28000.2705'
    }
    foreach ($packageId in $nuGetVersions.Keys) {
        Write-JsonFixture -Path (Join-Path $metadataSnapshotDirectory "nuget/$packageId.index.json") -Value ([ordered]@{
            versions = @($nuGetVersions[$packageId], "$($nuGetVersions[$packageId])-preview.1")
        })
    }

    $manifestPaths = @(
        Join-Path $fixtureRepositoryRoot 'vcpkg.json'
        Join-Path $fixtureRepositoryRoot 'Directory.Packages.props'
    )
    $originalVcpkgManifestContent = [System.IO.File]::ReadAllText($manifestPaths[0])
    $originalCentralPackageVersionsContent =
        [System.IO.File]::ReadAllText($manifestPaths[1])
    $originalHashes = @{}
    foreach ($manifestPath in $manifestPaths) {
        $originalHashes[$manifestPath] = (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    }

    $currentResult = Invoke-FreshnessCheck
    if ($currentResult.exitCode -ne 0) {
        throw "Current stable pins should pass freshness verification.`n$($currentResult.output)"
    }
    $expectedNoActionableUpdateStatement =
        'No newer dependency versions actionable through the approved package authorities were found.'
    if ($currentResult.output.IndexOf(
            $expectedNoActionableUpdateStatement,
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw "Current-pin result lacks a precise success statement.`n$($currentResult.output)"
    }
    if ($currentResult.output -match '(?i)preview') {
        throw "Prerelease metadata leaked into the stable freshness report.`n$($currentResult.output)"
    }
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # NuGet's flat-container index is the package authority for an exact
    # PackageVersion. A syntactically valid pin above every published version
    # is unavailable, not current.
    $unavailableNuGetPinContent = $originalCentralPackageVersionsContent.Replace(
        'PackageVersion Include="Microsoft.WindowsAppSDK" Version="2.4.0"',
        'PackageVersion Include="Microsoft.WindowsAppSDK" Version="2.5.0"'
    )
    if ($unavailableNuGetPinContent -ceq $originalCentralPackageVersionsContent) {
        throw 'The freshness fixture lacks the expected Microsoft.WindowsAppSDK pin.'
    }
    [System.IO.File]::WriteAllText(
        $manifestPaths[1],
        $unavailableNuGetPinContent,
        [System.Text.UTF8Encoding]::new($false)
    )

    $unavailableNuGetPinResult = Invoke-FreshnessCheck
    if ($unavailableNuGetPinResult.exitCode -eq 0) {
        throw (
            'Dependency freshness accepted a Windows App SDK pin absent from official NuGet.' +
            "`n$($unavailableNuGetPinResult.output)"
        )
    }
    $normalizedUnavailableNuGetPinOutput =
        $unavailableNuGetPinResult.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
    $requiredUnavailableNuGetPinDiagnostic =
        'Microsoft.WindowsAppSDK pins 2.5.0, but official NuGet does not contain that exact version.'
    if ($normalizedUnavailableNuGetPinOutput.IndexOf(
            $requiredUnavailableNuGetPinDiagnostic,
            [System.StringComparison]::OrdinalIgnoreCase
        ) -lt 0) {
        throw (
            'An unavailable NuGet pin failed without the required authority diagnostic.' +
            "`n$($unavailableNuGetPinResult.output)"
        )
    }
    [System.IO.File]::WriteAllText(
        $manifestPaths[1],
        $originalCentralPackageVersionsContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # Every authority has a documented top-level schema. Validate the raw root
    # before member access so JSON null/array/object substitutions cannot flow
    # into PowerShell's permissive property enumeration or coercion behavior.
    Assert-RejectedMetadataRoot `
        -Path (Join-Path $metadataSnapshotDirectory 'vcpkg/libjpeg-turbo.json') `
        -InvalidJson '[]' `
        -ExpectedDiagnostic (
            'official vcpkg registry metadata for libjpeg-turbo snapshot root must be a JSON object'
        )
    Assert-RejectedMetadataRoot `
        -Path (Join-Path $metadataSnapshotDirectory 'github/libjpeg-turbo.releases.json') `
        -InvalidJson '{}' `
        -ExpectedDiagnostic (
            'upstream GitHub releases for libjpeg-turbo snapshot root must be a JSON array'
        )
    Assert-RejectedMetadataRoot `
        -Path (Join-Path $metadataSnapshotDirectory 'nuget/microsoft.windowsappsdk.index.json') `
        -InvalidJson '[]' `
        -ExpectedDiagnostic (
            'official NuGet metadata for Microsoft.WindowsAppSDK snapshot root must be a JSON object'
        )

    # Dependency freshness consumes the repository manifest directly. A
    # duplicate override version must be rejected before ConvertFrom-Json can
    # silently keep the reviewed-looking last member and discard its sibling.
    $duplicateOverrideVersionManifestContent = $originalVcpkgManifestContent.Replace(
        '"version": "3.2.0"',
        '"version": "0.0.1",' + "`n" + '      "version": "3.2.0"'
    )
    if ($duplicateOverrideVersionManifestContent -ceq $originalVcpkgManifestContent) {
        throw 'The freshness fixture lacks the expected libjpeg-turbo override version.'
    }
    [System.IO.File]::WriteAllText(
        $manifestPaths[0],
        $duplicateOverrideVersionManifestContent,
        [System.Text.UTF8Encoding]::new($false)
    )

    $duplicateOverrideVersionResult = Invoke-FreshnessCheck
    if ($duplicateOverrideVersionResult.exitCode -eq 0) {
        throw (
            'Dependency freshness accepted a duplicate vcpkg override version.' +
            "`n$($duplicateOverrideVersionResult.output)"
        )
    }
    $normalizedDuplicateOverrideVersionOutput =
        $duplicateOverrideVersionResult.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
    if ($normalizedDuplicateOverrideVersionOutput.IndexOf(
            "Duplicate property 'version'",
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw (
            'Dependency freshness rejected duplicate JSON without identifying the version member.' +
            "`n$($duplicateOverrideVersionResult.output)"
        )
    }

    [System.IO.File]::WriteAllText(
        $manifestPaths[0],
        $originalVcpkgManifestContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # Port revisions are part of an exact vcpkg version: packaging revision #1
    # is newer than revision #0 even when the upstream version is unchanged.
    $libjpegTurboRegistryMetadataPath = Join-Path $metadataSnapshotDirectory 'vcpkg/libjpeg-turbo.json'
    Write-JsonFixture -Path $libjpegTurboRegistryMetadataPath -Value ([ordered]@{
        versions = @(
            [ordered]@{
                'version-semver' = '3.2.0'
                'port-version' = 0
                'git-tree' = '0123456789abcdef0123456789abcdef01234567'
            },
            [ordered]@{
                'version-semver' = '3.2.0'
                'port-version' = 1
                'git-tree' = '1123456789abcdef0123456789abcdef01234567'
            }
        )
    })

    $outdatedPortRevisionResult = Invoke-FreshnessCheck
    if ($outdatedPortRevisionResult.exitCode -eq 0) {
        throw "A newer vcpkg port revision must make the freshness gate fail.`n$($outdatedPortRevisionResult.output)"
    }
    $normalizedPortRevisionOutput = $outdatedPortRevisionResult.output -replace '\s+', ' '
    foreach ($requiredPortRevisionDiagnostic in @(
        'libjpeg-turbo',
        'pinned 3.2.0;',
        'offers 3.2.0#1',
        'official vcpkg registry'
    )) {
        if ($normalizedPortRevisionOutput.IndexOf(
                $requiredPortRevisionDiagnostic,
                [System.StringComparison]::OrdinalIgnoreCase
            ) -lt 0) {
            throw "Port-revision report omits '$requiredPortRevisionDiagnostic'.`n$($outdatedPortRevisionResult.output)"
        }
    }
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # The current vcpkg manifest syntax places a nonzero port revision in the
    # version string itself. Prove that an exact #N override parses, compares,
    # and remains immutable when it already matches the newest registry entry.
    $portRevisionPinnedManifest =
        Get-Content -LiteralPath $manifestPaths[0] -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $libjpegTurboOverride = @(
        $portRevisionPinnedManifest.overrides |
            Where-Object { [string]$_.name -ceq 'libjpeg-turbo' }
    )
    if ($libjpegTurboOverride.Count -ne 1) {
        throw 'The freshness fixture must contain exactly one libjpeg-turbo override.'
    }
    $libjpegTurboOverride[0]['version'] = '3.2.0#1'
    Write-JsonFixture -Path $manifestPaths[0] -Value $portRevisionPinnedManifest

    $portRevisionPinnedHashes = @{}
    foreach ($manifestPath in $manifestPaths) {
        $portRevisionPinnedHashes[$manifestPath] =
            (Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash
    }
    $currentPortRevisionResult = Invoke-FreshnessCheck
    if ($currentPortRevisionResult.exitCode -ne 0) {
        throw "A current exact #N port-revision pin should pass freshness verification.`n$($currentPortRevisionResult.output)"
    }
    Assert-FileHashUnchanged -ExpectedHashes $portRevisionPinnedHashes

    [System.IO.File]::WriteAllText(
        $manifestPaths[0],
        $originalVcpkgManifestContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Write-JsonFixture -Path $libjpegTurboRegistryMetadataPath -Value ([ordered]@{
        versions = @(
            [ordered]@{
                'version-semver' = '3.2.0'
                'port-version' = 0
                'git-tree' = '0123456789abcdef0123456789abcdef01234567'
            }
        )
    })
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # A stable release in either authoritative channel must become actionable.
    Write-JsonFixture -Path (Join-Path $metadataSnapshotDirectory 'vcpkg/exiv2.json') -Value ([ordered]@{
        versions = @(
            [ordered]@{ 'version-semver' = '0.28.9'; 'port-version' = 0; 'git-tree' = '1123456789abcdef0123456789abcdef01234567' },
            [ordered]@{ 'version-semver' = '0.28.8'; 'port-version' = 0; 'git-tree' = '0123456789abcdef0123456789abcdef01234567' }
        )
    })
    Write-JsonFixture -Path (Join-Path $metadataSnapshotDirectory 'github/exiv2.releases.json') -Value @(
        [ordered]@{ tag_name = 'v0.28.9'; draft = $false; prerelease = $false },
        [ordered]@{ tag_name = 'v0.29.0-rc1'; draft = $false; prerelease = $true }
    )

    $outdatedResult = Invoke-FreshnessCheck
    if ($outdatedResult.exitCode -eq 0) {
        throw "A newer stable Exiv2 release must make the freshness gate fail.`n$($outdatedResult.output)"
    }
    $normalizedOutdatedOutput = $outdatedResult.output -replace '\s+', ' '
    foreach ($requiredDiagnostic in @('exiv2', '0.28.8', '0.28.9', 'official vcpkg registry', 'upstream GitHub')) {
        if ($normalizedOutdatedOutput.IndexOf($requiredDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "Outdated-pin report omits '$requiredDiagnostic'.`n$($outdatedResult.output)"
        }
    }
    if ($outdatedResult.output -match '(?i)0\.29\.0-rc1') {
        throw "Prerelease Exiv2 metadata must not appear in an upgrade report.`n$($outdatedResult.output)"
    }
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # An upstream release that the selected official package registry cannot
    # resolve is important channel-lag information, not an actionable manifest
    # update. The gate must not encourage an unreviewed overlay or source shim.
    Write-JsonFixture -Path (Join-Path $metadataSnapshotDirectory 'vcpkg/exiv2.json') -Value ([ordered]@{
        versions = @(
            [ordered]@{ 'version-semver' = '0.28.8'; 'port-version' = 0; 'git-tree' = '0123456789abcdef0123456789abcdef01234567' }
        )
    })
    $registryLagResult = Invoke-FreshnessCheck
    if ($registryLagResult.exitCode -ne 0) {
        throw "An upstream-only release must be reported without proposing an unrestorable vcpkg pin.`n$($registryLagResult.output)"
    }
    $normalizedRegistryLagOutput = $registryLagResult.output -replace '\s+', ' '
    foreach ($requiredLagDiagnostic in @('exiv2', '0.28.9', 'not yet present in the official vcpkg registry')) {
        if ($normalizedRegistryLagOutput.IndexOf($requiredLagDiagnostic, [System.StringComparison]::OrdinalIgnoreCase) -lt 0) {
            throw "Registry-lag report omits '$requiredLagDiagnostic'.`n$($registryLagResult.output)"
        }
    }
    if ($registryLagResult.output.IndexOf(
            $expectedNoActionableUpdateStatement,
            [System.StringComparison]::Ordinal
        ) -lt 0) {
        throw "Registry-lag result lacks the package-authority success boundary.`n$($registryLagResult.output)"
    }
    if ($registryLagResult.output -match 'No newer stable dependency versions were found') {
        throw "Registry-lag result contradicts its upstream release notice.`n$($registryLagResult.output)"
    }
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    # A stable upstream tag is not a resolvable package version until the
    # approved vcpkg registry's version database contains the exact upstream
    # version and port revision. A false-green here would bless a manifest that
    # vcpkg itself rejects as unavailable.
    $unavailableRegistryPinManifest =
        Get-Content -LiteralPath $manifestPaths[0] -Raw |
        ConvertFrom-Json -AsHashtable -Depth 20
    $exiv2Override = @(
        $unavailableRegistryPinManifest.overrides |
            Where-Object { [string]$_.name -ceq 'exiv2' }
    )
    if ($exiv2Override.Count -ne 1) {
        throw 'The freshness fixture must contain exactly one Exiv2 override.'
    }
    $exiv2Override[0].version = '0.28.9'
    Write-JsonFixture -Path $manifestPaths[0] -Value $unavailableRegistryPinManifest

    $unavailableRegistryPinResult = Invoke-FreshnessCheck
    if ($unavailableRegistryPinResult.exitCode -eq 0) {
        throw (
            'Dependency freshness accepted an Exiv2 pin absent from the official vcpkg registry.' +
            "`n$($unavailableRegistryPinResult.output)"
        )
    }
    $normalizedUnavailableRegistryPinOutput =
        $unavailableRegistryPinResult.output -replace '\s+\|\s+', ' ' -replace '\s+', ' '
    $requiredUnavailableRegistryPinDiagnostic =
        'exiv2 pins 0.28.9, but official vcpkg registry does not contain that exact version.'
    if ($normalizedUnavailableRegistryPinOutput.IndexOf(
            $requiredUnavailableRegistryPinDiagnostic,
            [System.StringComparison]::OrdinalIgnoreCase
        ) -lt 0) {
        throw (
            'An unavailable vcpkg pin failed without the required authority diagnostic.' +
            "`n$($unavailableRegistryPinResult.output)"
        )
    }
    [System.IO.File]::WriteAllText(
        $manifestPaths[0],
        $originalVcpkgManifestContent,
        [System.Text.UTF8Encoding]::new($false)
    )
    Assert-FileHashUnchanged -ExpectedHashes $originalHashes

    Write-Output (
        'PASS: freshness verification requires authority-published exact pins, compares vcpkg port ' +
        'revisions, ignores prereleases, rejects ambiguous JSON, distinguishes actionable upgrades ' +
        'from release-channel lag, and never mutates manifests.'
    )
}
finally {
    if (Test-Path -LiteralPath $temporaryRoot) {
        $resolvedTemporaryRoot = [System.IO.Path]::GetFullPath($temporaryRoot)
        $resolvedOperatingSystemTemporaryRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath())
        if (-not $resolvedTemporaryRoot.StartsWith($resolvedOperatingSystemTemporaryRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove test directory outside the operating-system temporary root: $resolvedTemporaryRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRoot -Recurse -Force
    }
}
