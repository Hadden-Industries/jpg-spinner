[CmdletBinding()]
param(
    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Import-Module (
    Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1'
) -Force

$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $repositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $repositoryRoot"
}

$resolutionFailures = [System.Collections.Generic.List[string]]::new()

function Add-ResolutionFailure {
    param(
        [Parameter(Mandatory)]
        [string]$Message
    )

    [void]$resolutionFailures.Add($Message)
}

function Get-RepositoryPath {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    return Join-Path -Path $repositoryRoot -ChildPath $RelativePath
}

function Read-NuGetLockFile {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    $lockFilePath = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $lockFilePath -PathType Leaf)) {
        Add-ResolutionFailure -Message "Required NuGet lock file is absent: $RelativePath"
        return $null
    }

    try {
        # Validate the source text before PowerShell conversion: lock files are
        # dependency authorities, and ConvertFrom-Json otherwise applies lossy
        # last-member-wins behavior to duplicate JSON members.
        $lockFile = ConvertFrom-JsonWithUniqueObjectMembers `
            -JsonText ([System.IO.File]::ReadAllText($lockFilePath)) `
            -SourceDescription "$RelativePath NuGet lock file" `
            -RequiredRootValueKind Object `
            -AsHashtable `
            -MaximumDepth 100
    }
    catch {
        Add-ResolutionFailure -Message $_.Exception.Message
        return $null
    }

    # NuGet's lock format is explicitly versioned. Central Package Management
    # produces version 2 for this graph, and dependencies must remain an object
    # keyed by target graph; do not let PowerShell coerce strings or arrays into
    # values that merely compare equal.
    $lockFileVersionIsInteger =
        $lockFile.Contains('version') -and
        $lockFile['version'] -is [System.ValueType] -and
        $lockFile['version'] -isnot [bool] -and
        $lockFile['version'] -isnot [decimal] -and
        $lockFile['version'] -isnot [double] -and
        $lockFile['version'] -isnot [single]
    if (-not $lockFileVersionIsInteger -or $lockFile['version'] -cne 2) {
        Add-ResolutionFailure -Message (
            "$RelativePath must declare integer NuGet lock-file version 2."
        )
    }
    if (
        -not $lockFile.Contains('dependencies') -or
        $lockFile['dependencies'] -isnot [System.Collections.IDictionary]
    ) {
        Add-ResolutionFailure -Message (
            "$RelativePath dependencies must be a JSON object keyed by target graph."
        )
    }

    return $lockFile
}

$repositoryBuildToolchain = $null
try {
    $repositoryBuildToolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') `
        -RepositoryRoot $repositoryRoot
    [void][System.Reflection.Assembly]::LoadFrom(
        $repositoryBuildToolchain.msBuildAssemblyPath
    )
}
catch {
    Add-ResolutionFailure -Message "Could not resolve the repository build toolchain: $($_.Exception.Message)"
}

function Read-MSBuildProjectRootElement {
    param(
        [Parameter(Mandatory)]
        [string]$RelativePath
    )

    if ($null -eq $repositoryBuildToolchain) {
        return $null
    }

    $path = Get-RepositoryPath -RelativePath $RelativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        Add-ResolutionFailure -Message "Required MSBuild file is absent: $RelativePath"
        return $null
    }

    try {
        return [Microsoft.Build.Construction.ProjectRootElement]::Open($path)
    }
    catch {
        Add-ResolutionFailure -Message "$RelativePath is not valid MSBuild XML: $($_.Exception.Message)"
        return $null
    }
}

function Test-OrdinalIgnoreCaseIdentityEquality {
    param(
        [Parameter(Mandatory)]
        [string]$Left,

        [Parameter(Mandatory)]
        [string]$Right
    )

    return [string]::Equals(
        $Left,
        $Right,
        [System.StringComparison]::OrdinalIgnoreCase
    )
}

function Test-OrdinalIgnoreCaseIdentityCollectionContains {
    param(
        [Parameter(Mandatory)]
        [System.Collections.IEnumerable]$Identities,

        [Parameter(Mandatory)]
        [string]$Candidate
    )

    foreach ($identity in $Identities) {
        if (Test-OrdinalIgnoreCaseIdentityEquality `
                -Left ([string]$identity) `
                -Right $Candidate) {
            return $true
        }
    }
    return $false
}

function Get-UniquePackageReferenceMetadataValue {
    param(
        [Parameter(Mandatory)]
        [object]$PackageReference,

        [Parameter(Mandatory)]
        [string]$MetadataName,

        [Parameter(Mandatory)]
        [string]$SourceDescription
    )

    $matchingMetadata = @(
        $PackageReference.Metadata |
            Where-Object {
                Test-OrdinalIgnoreCaseIdentityEquality `
                    -Left $_.Name `
                    -Right $MetadataName
            }
    )
    if ($matchingMetadata.Count -eq 0) {
        return $null
    }
    if ($matchingMetadata.Count -ne 1) {
        Add-ResolutionFailure -Message (
            "$SourceDescription must declare MSBuild metadata '$MetadataName' " +
            "at most once; found $($matchingMetadata.Count) case-equivalent declarations."
        )
        return $null
    }
    return [string]$matchingMetadata[0].Value
}

$expectedCentralPackageVersions = [ordered]@{
    'Microsoft.WindowsAppSDK' = '2.5.1'
    'Microsoft.Windows.CppWinRT' = '3.0.260818.1'
    'Microsoft.Windows.SDK.BuildTools' = '10.0.28000.2705'
}
$expectedProjectPackageReferences = [ordered]@{
    'src/JpgSpinner.App/JpgSpinner.App.vcxproj' = @(
        'Microsoft.WindowsAppSDK',
        'Microsoft.Windows.CppWinRT',
        'Microsoft.Windows.SDK.BuildTools'
    )
    'src/JpgSpinner.BatchProcessing/JpgSpinner.BatchProcessing.vcxproj' = @()
    'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj' = @()
    'src/JpgSpinner.JpegTransformation/JpgSpinner.JpegTransformation.vcxproj' = @()
    'src/JpgSpinner.WindowsStorage/JpgSpinner.WindowsStorage.vcxproj' = @(
        'Microsoft.Windows.CppWinRT'
    )
    'tests/JpgSpinner.BatchProcessing.Tests/JpgSpinner.BatchProcessing.Tests.vcxproj' = @()
    'tests/JpgSpinner.Domain.Tests/JpgSpinner.Domain.Tests.vcxproj' = @()
    'tests/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.vcxproj' = @()
    'tests/JpgSpinner.Presentation.Tests/JpgSpinner.Presentation.Tests.vcxproj' = @()
    'tests/JpgSpinner.WindowsStorage.Tests/JpgSpinner.WindowsStorage.Tests.vcxproj' = @(
        'Microsoft.Windows.CppWinRT'
    )
    'tests/TestSupport/TestSupport.vcxproj' = @()
}

$centralPackageVersions = Read-MSBuildProjectRootElement -RelativePath 'Directory.Packages.props'
if ($null -ne $centralPackageVersions) {
    foreach ($requiredCentralProperty in ([ordered]@{
        ManagePackageVersionsCentrally = 'true'
        RestorePackagesWithLockFile = 'true'
        CentralPackageVersionOverrideEnabled = 'false'
    }).GetEnumerator()) {
        $matchingProperties = @(
            $centralPackageVersions.Properties |
                Where-Object {
                    Test-OrdinalIgnoreCaseIdentityEquality `
                        -Left $_.Name `
                        -Right $requiredCentralProperty.Key
                }
        )
        if (
            $matchingProperties.Count -ne 1 -or
            $matchingProperties[0].Value -cne $requiredCentralProperty.Value
        ) {
            Add-ResolutionFailure -Message (
                "Directory.Packages.props must declare $($requiredCentralProperty.Key) " +
                "'$($requiredCentralProperty.Value)' exactly once."
            )
        }
    }

    $centralPackageVersionItems = @(
        $centralPackageVersions.Items |
            Where-Object {
                Test-OrdinalIgnoreCaseIdentityEquality `
                    -Left $_.ItemType `
                    -Right 'PackageVersion'
            }
    )
    foreach ($expectedPackageVersion in $expectedCentralPackageVersions.GetEnumerator()) {
        $matchingPackageVersions = @(
            $centralPackageVersionItems |
                Where-Object {
                    Test-OrdinalIgnoreCaseIdentityEquality `
                        -Left $_.Include `
                        -Right $expectedPackageVersion.Key
                }
        )
        if (
            $matchingPackageVersions.Count -ne 1 -or
            (Get-UniquePackageReferenceMetadataValue `
                -PackageReference $matchingPackageVersions[0] `
                -MetadataName 'Version' `
                -SourceDescription (
                    "Directory.Packages.props PackageVersion '$($expectedPackageVersion.Key)'"
                )) -cne $expectedPackageVersion.Value
        ) {
            Add-ResolutionFailure -Message (
                "Directory.Packages.props must pin $($expectedPackageVersion.Key) " +
                "'$($expectedPackageVersion.Value)' exactly once."
            )
        }
    }
    foreach ($centralPackageVersionItem in $centralPackageVersionItems) {
        if (-not (Test-OrdinalIgnoreCaseIdentityCollectionContains `
                -Identities $expectedCentralPackageVersions.Keys `
                -Candidate $centralPackageVersionItem.Include)) {
            Add-ResolutionFailure -Message (
                "Directory.Packages.props contains unapproved package version " +
                "'$($centralPackageVersionItem.Include)'."
            )
        }
    }
}

foreach ($expectedProjectPackages in $expectedProjectPackageReferences.GetEnumerator()) {
    $project = Read-MSBuildProjectRootElement -RelativePath $expectedProjectPackages.Key
    if ($null -eq $project) {
        continue
    }

    $packageReferences = @(
        $project.Items | Where-Object {
            Test-OrdinalIgnoreCaseIdentityEquality `
                -Left $_.ItemType `
                -Right 'PackageReference'
        }
    )
    foreach ($expectedPackageReference in $expectedProjectPackages.Value) {
        $matchingPackageReferences = @(
            $packageReferences | Where-Object {
                Test-OrdinalIgnoreCaseIdentityEquality `
                    -Left $_.Include `
                    -Right $expectedPackageReference
            }
        )
        if ($matchingPackageReferences.Count -ne 1) {
            Add-ResolutionFailure -Message (
                "$($expectedProjectPackages.Key) must reference $expectedPackageReference " +
                "exactly once without a project-local version; found $($matchingPackageReferences.Count)."
            )
            continue
        }

        foreach ($forbiddenVersionMetadataName in @('Version', 'VersionOverride')) {
            $forbiddenVersion = Get-UniquePackageReferenceMetadataValue `
                -PackageReference $matchingPackageReferences[0] `
                -MetadataName $forbiddenVersionMetadataName `
                -SourceDescription (
                    "$($expectedProjectPackages.Key) PackageReference '$expectedPackageReference'"
                )
            if (-not [string]::IsNullOrWhiteSpace($forbiddenVersion)) {
                Add-ResolutionFailure -Message (
                    "$($expectedProjectPackages.Key) PackageReference '$expectedPackageReference' " +
                    "must not declare $forbiddenVersionMetadataName; central package policy owns its version."
                )
            }
        }
    }
    foreach ($packageReference in $packageReferences) {
        if (-not (Test-OrdinalIgnoreCaseIdentityCollectionContains `
                -Identities $expectedProjectPackages.Value `
                -Candidate $packageReference.Include)) {
            Add-ResolutionFailure -Message (
                "$($expectedProjectPackages.Key) contains unapproved PackageReference " +
                "'$($packageReference.Include)'."
            )
        }
    }

    $lockFileRelativePath = Join-Path `
        (Split-Path -Parent $expectedProjectPackages.Key) `
        'packages.lock.json'
    $lockFilePath = Get-RepositoryPath -RelativePath $lockFileRelativePath
    if (-not (Test-Path -LiteralPath $lockFilePath -PathType Leaf)) {
        Add-ResolutionFailure -Message (
            "$($expectedProjectPackages.Key) has no NuGet packages.lock.json evidence."
        )
    }
    else {
        [void](Read-NuGetLockFile -RelativePath $lockFileRelativePath)
    }
}

if ($resolutionFailures.Count -gt 0) {
    Write-Error "NuGet resolution policy failed with $($resolutionFailures.Count) violation(s):`n - $($resolutionFailures -join "`n - ")"
    exit 1
}

$repositoryNuGetConfigurationPath = Get-RepositoryPath -RelativePath 'NuGet.config'
$packageConsumerProjectPaths = @(
    $expectedProjectPackageReferences.Keys |
        ForEach-Object { Get-RepositoryPath -RelativePath $_ }
)
$lockedPackageGraphPaths = @(
    $expectedProjectPackageReferences.Keys |
        ForEach-Object {
            Get-RepositoryPath -RelativePath (
                Join-Path (Split-Path -Parent $_) 'packages.lock.json'
            )
        }
)
$lockFileHashesBeforeRestore = @{}
foreach ($lockedPackageGraphPath in $lockedPackageGraphPaths) {
    $lockFileHashesBeforeRestore[$lockedPackageGraphPath] =
        (Get-FileHash -LiteralPath $lockedPackageGraphPath -Algorithm SHA256).Hash
}

$temporaryRestoreRoot = Join-Path (
    [System.IO.Path]::GetTempPath()
) "jpg-spinner-nuget-resolution-$([guid]::NewGuid().ToString('N'))"
try {
    [void][System.IO.Directory]::CreateDirectory($temporaryRestoreRoot)
    $isolatedPackagesPath = Join-Path $temporaryRestoreRoot 'packages'
    $isolatedHttpCachePath = Join-Path $temporaryRestoreRoot 'nuget\http-cache'
    $isolatedPluginCachePath = Join-Path $temporaryRestoreRoot 'nuget\plugins-cache'
    $isolatedScratchPath = Join-Path $temporaryRestoreRoot 'nuget\scratch'
    foreach ($isolatedDirectoryPath in @(
        $isolatedPackagesPath,
        $isolatedHttpCachePath,
        $isolatedPluginCachePath,
        $isolatedScratchPath
    )) {
        [void][System.IO.Directory]::CreateDirectory($isolatedDirectoryPath)
    }

    foreach ($platform in @('Win32', 'x64', 'ARM64')) {
        foreach ($packageConsumerProjectPath in $packageConsumerProjectPaths) {
            $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
            $startInfo.FileName = $repositoryBuildToolchain.msBuildExecutablePath
            $startInfo.WorkingDirectory = $repositoryRoot
            $startInfo.UseShellExecute = $false
            $startInfo.RedirectStandardOutput = $true
            $startInfo.RedirectStandardError = $true

            # Use NuGet's documented cache environment variables rather than
            # deleting or borrowing any developer-wide caches. All disposable
            # restore state then shares one containment-checked temporary root.
            $startInfo.Environment['NUGET_PACKAGES'] = $isolatedPackagesPath
            $startInfo.Environment['NUGET_HTTP_CACHE_PATH'] = $isolatedHttpCachePath
            $startInfo.Environment['NUGET_PLUGINS_CACHE_PATH'] = $isolatedPluginCachePath
            $startInfo.Environment['NUGET_SCRATCH'] = $isolatedScratchPath
            foreach ($argument in @(
                $packageConsumerProjectPath,
                '-nologo',
                '-noAutoResponse',
                '-m:1',
                '-t:Restore',
                '-verbosity:minimal',
                '-p:Configuration=Debug',
                "-p:Platform=$platform",
                "-p:RestorePackagesPath=$isolatedPackagesPath",
                # RestoreConfigFile is NuGet's native project-level isolation
                # boundary: machine and user configs cannot add feeds or
                # credentials to this graph.
                "-p:RestoreConfigFile=$repositoryNuGetConfigurationPath",
                '-p:RestoreLockedMode=true',
                '-p:ContinuousIntegrationBuild=true',
                # NuGet resolution is independent from the native vcpkg graph.
                '-p:VcpkgEnabled=false'
            )) {
                [void]$startInfo.ArgumentList.Add($argument)
            }

            $process = [System.Diagnostics.Process]::new()
            $process.StartInfo = $startInfo
            [void]$process.Start()
            $standardOutputReadTask = $process.StandardOutput.ReadToEndAsync()
            $standardErrorReadTask = $process.StandardError.ReadToEndAsync()
            $process.WaitForExit()
            $standardOutput = $standardOutputReadTask.GetAwaiter().GetResult()
            $standardError = $standardErrorReadTask.GetAwaiter().GetResult()
            if ($process.ExitCode -ne 0) {
                $diagnosticText = @($standardError.Trim(), $standardOutput.Trim()) |
                    Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
                throw (
                    "Locked NuGet restore failed for '$packageConsumerProjectPath' " +
                    "on platform '$platform'.`n" +
                    ($diagnosticText -join "`n")
                )
            }
        }
    }

    foreach ($lockedPackageGraphPath in $lockedPackageGraphPaths) {
        $hashAfterRestore =
            (Get-FileHash -LiteralPath $lockedPackageGraphPath -Algorithm SHA256).Hash
        if ($hashAfterRestore -cne $lockFileHashesBeforeRestore[$lockedPackageGraphPath]) {
            throw (
                'Locked restore changed committed dependency evidence: ' +
                $lockedPackageGraphPath
            )
        }
    }
}
finally {
    if (Test-Path -LiteralPath $temporaryRestoreRoot -PathType Container) {
        $resolvedTemporaryRestoreRoot = [System.IO.Path]::GetFullPath($temporaryRestoreRoot)
        $resolvedOperatingSystemTemporaryRoot = [System.IO.Path]::GetFullPath(
            [System.IO.Path]::GetTempPath()
        )
        if (-not $resolvedTemporaryRestoreRoot.StartsWith(
            $resolvedOperatingSystemTemporaryRoot,
            [System.StringComparison]::OrdinalIgnoreCase
        )) {
            throw "Refusing to remove NuGet evidence outside the temporary root: $resolvedTemporaryRestoreRoot"
        }
        Remove-Item -LiteralPath $resolvedTemporaryRestoreRoot -Recurse -Force
    }
}

Write-Output (
    'NuGet resolution verified for Win32, x64, and ARM64: Windows App SDK 2.5.1, ' +
    'C++/WinRT 3.0.260818.1, and Windows SDK BuildTools 10.0.28000.2705.'
)
