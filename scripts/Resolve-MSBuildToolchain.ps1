[CmdletBinding()]
param(
    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$jsonObjectMemberValidationModulePath =
    Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1'
Import-Module -Name $jsonObjectMemberValidationModulePath -Force

$resolvedRepositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $resolvedRepositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $resolvedRepositoryRoot"
}

$programFilesX86 = [Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFilesX86)
$vswherePath = Join-Path $programFilesX86 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath -PathType Leaf)) {
    throw "Visual Studio Installer's supported discovery utility is absent: $vswherePath"
}

# Omitting -prerelease is intentional: vswhere then limits discovery to stable
# instances. The parsed flags below provide a second, explicit policy check.
$instanceJson = & $vswherePath `
    -products '*' `
    -requires 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64' `
    -requires 'Microsoft.VisualStudio.Component.VC.Tools.ARM64' `
    -requires 'Microsoft.VisualStudio.Component.VC.CMake.Project' `
    -format json `
    -utf8
if ($LASTEXITCODE -ne 0) {
    throw "vswhere failed with exit code $LASTEXITCODE."
}

$instanceJsonText = $instanceJson -join [Environment]::NewLine
$instances = @(
    ConvertFrom-JsonWithUniqueObjectMembers `
        -JsonText $instanceJsonText `
        -SourceDescription 'vswhere instance discovery response' `
        -RequiredRootValueKind Array `
        -MaximumDepth 100
)
$eligibleInstances = @(
    $instances |
        Where-Object {
            $_.isComplete -eq $true -and
            $_.isLaunchable -eq $true -and
            $_.isPrerelease -eq $false -and
            [version]$_.installationVersion -ge [version]'18.0' -and
            [version]$_.installationVersion -lt [version]'19.0' -and
            [string]$_.channelId -ceq 'VisualStudio.18.Release'
        } |
        Sort-Object -Property @{ Expression = { [version]$_.installationVersion }; Descending = $true }
)
if ($eligibleInstances.Count -eq 0) {
    throw 'No complete, launchable, stable Visual Studio 18 instance with x86/x64 and ARM64 C++ tools is installed.'
}

$selectedInstance = $eligibleInstances[0]
$visualStudioInstallationPath = [System.IO.Path]::GetFullPath([string]$selectedInstance.installationPath)
$msBuildExecutablePath = Join-Path $visualStudioInstallationPath 'MSBuild/Current/Bin/amd64/MSBuild.exe'
if (-not (Test-Path -LiteralPath $msBuildExecutablePath -PathType Leaf)) {
    throw "The selected Visual Studio instance lacks 64-bit MSBuild: $msBuildExecutablePath"
}

# Resolve every build-language authority from the same selected Visual Studio
# instance. An assembly from a separate NuGet cache or a CMake found through
# PATH could interpret the repository differently from the locked build tools.
$msBuildAssemblyPath = Join-Path (
    Split-Path -Parent $msBuildExecutablePath
) 'Microsoft.Build.dll'
if (-not (Test-Path -LiteralPath $msBuildAssemblyPath -PathType Leaf)) {
    throw "The selected Visual Studio instance lacks the MSBuild construction assembly: $msBuildAssemblyPath"
}

$cmakeExecutablePath = Join-Path $visualStudioInstallationPath (
    'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
)
if (-not (Test-Path -LiteralPath $cmakeExecutablePath -PathType Leaf)) {
    throw "The selected Visual Studio instance lacks its bundled CMake executable: $cmakeExecutablePath"
}

$vcToolsRoot = Join-Path $visualStudioInstallationPath 'VC/Tools/MSVC'
if (-not (Test-Path -LiteralPath $vcToolsRoot -PathType Container)) {
    throw "The selected Visual Studio instance lacks an MSVC tool directory: $vcToolsRoot"
}

$toolchainLockPath = Join-Path $resolvedRepositoryRoot 'eng/toolchain-lock.json'
$lockedVcToolsVersion = $null
if (Test-Path -LiteralPath $toolchainLockPath -PathType Leaf) {
    try {
        $toolchainLockJsonText = Get-Content -LiteralPath $toolchainLockPath -Raw
        $toolchainLock = ConvertFrom-JsonWithUniqueObjectMembers `
            -JsonText $toolchainLockJsonText `
            -SourceDescription 'eng/toolchain-lock.json' `
            -RequiredRootValueKind Object `
            -AsHashtable `
            -MaximumDepth 20
    }
    catch {
        throw "The existing toolchain lock is invalid JSON: $($_.Exception.Message)"
    }

    if ($toolchainLock.schemaVersion -ne 1) {
        throw "Unsupported toolchain lock schemaVersion '$($toolchainLock.schemaVersion)'."
    }
    if ($toolchainLock.visualStudioMajorVersion -ne 18) {
        throw (
            "The toolchain lock requires Visual Studio major version " +
            "'$($toolchainLock.visualStudioMajorVersion)', not 18."
        )
    }
    if ([string]$toolchainLock.platformToolset -cne 'v145') {
        throw "The toolchain lock requires PlatformToolset '$($toolchainLock.platformToolset)', not v145."
    }

    $lockedVcToolsVersion = [string]$toolchainLock.vcToolsVersion
    if ($lockedVcToolsVersion -notmatch '^14\.51\.\d+$') {
        throw (
            'The toolchain lock vcToolsVersion is not an exact MSVC 14.51 ' +
            "servicing directory: '$lockedVcToolsVersion'."
        )
    }
}

$installedVcToolsVersions = @(
    Get-ChildItem -LiteralPath $vcToolsRoot -Directory |
        Where-Object { $_.Name -match '^14\.51\.\d+$' } |
        Sort-Object -Property @{ Expression = { [version]$_.Name }; Descending = $true }
)
if ($installedVcToolsVersions.Count -eq 0) {
    throw "The selected Visual Studio instance has no stable MSVC 14.51 servicing directory beneath $vcToolsRoot."
}

if ($null -ne $lockedVcToolsVersion) {
    $selectedVcToolsDirectory = @($installedVcToolsVersions | Where-Object Name -CEQ $lockedVcToolsVersion)
    if ($selectedVcToolsDirectory.Count -ne 1) {
        throw (
            "The exactly locked VCToolsVersion '$lockedVcToolsVersion' is not installed " +
            'in the selected Visual Studio instance.'
        )
    }
    $selectedVcToolsDirectory = $selectedVcToolsDirectory[0]
}
else {
    $selectedVcToolsDirectory = $installedVcToolsVersions[0]
}

$vcToolsDirectoryPath = [System.IO.Path]::GetFullPath($selectedVcToolsDirectory.FullName)
$targetTools = [ordered]@{}
foreach ($targetArchitecture in @('x86', 'x64', 'arm64')) {
    $targetBinDirectory = Join-Path $vcToolsDirectoryPath "bin/Hostx64/$targetArchitecture"
    $compilerExecutablePath = Join-Path $targetBinDirectory 'cl.exe'
    $linkerExecutablePath = Join-Path $targetBinDirectory 'link.exe'
    foreach ($requiredExecutablePath in @($compilerExecutablePath, $linkerExecutablePath)) {
        if (-not (Test-Path -LiteralPath $requiredExecutablePath -PathType Leaf)) {
            throw "The locked toolchain lacks required $targetArchitecture executable: $requiredExecutablePath"
        }
    }

    $targetTools[$targetArchitecture] = [pscustomobject][ordered]@{
        targetArchitecture = $targetArchitecture
        compilerExecutablePath = [System.IO.Path]::GetFullPath($compilerExecutablePath)
        linkerExecutablePath = [System.IO.Path]::GetFullPath($linkerExecutablePath)
    }
}

# A single immutable record keeps every consumer on the same selected instance
# and servicing directory. Callers never repeat discovery or infer tool paths.
[pscustomobject][ordered]@{
    PSTypeName = 'JpgSpinner.MSBuildToolchain'
    visualStudioInstanceId = [string]$selectedInstance.instanceId
    visualStudioInstallationVersion = [string]$selectedInstance.installationVersion
    visualStudioMajorVersion = 18
    visualStudioInstallationPath = $visualStudioInstallationPath
    visualStudioChannelId = [string]$selectedInstance.channelId
    isComplete = [bool]$selectedInstance.isComplete
    isPrerelease = [bool]$selectedInstance.isPrerelease
    platformToolset = 'v145'
    vcToolsVersion = [string]$selectedVcToolsDirectory.Name
    vcToolsDirectoryPath = $vcToolsDirectoryPath
    msBuildExecutablePath = [System.IO.Path]::GetFullPath($msBuildExecutablePath)
    msBuildAssemblyPath = [System.IO.Path]::GetFullPath($msBuildAssemblyPath)
    cmakeExecutablePath = [System.IO.Path]::GetFullPath($cmakeExecutablePath)
    targetTools = $targetTools
}
