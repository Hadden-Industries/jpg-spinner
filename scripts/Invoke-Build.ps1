[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration,

    [Parameter(Mandatory)]
    [ValidateSet('x86', 'x64', 'ARM64')]
    [string]$Architecture,

    # MSBuild process concurrency, not the number of compiler threads. Two is
    # the conservative default for this native solution; callers with a known
    # memory budget may choose a different positive count explicitly.
    [Parameter()]
    [ValidateRange(1, [int]::MaxValue)]
    [int]$MaximumBuildProcessCount = 2,

    # Select the native application project for packaging without building test
    # hosts into that graph; normal builds retain the whole modern solution.
    [ValidateSet('ModernSolution', 'PackagedApplication')]
    [string]$BuildEntryPoint = 'ModernSolution',

    # Requests an unsigned package from the native single-project MSIX targets.
    # Signing/installing/publishing are deliberately separate operations.
    [string]$UnsignedPackageOutputDirectory = '',

    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $repositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $repositoryRoot"
}

$packageOutputPath = $null
if (-not [string]::IsNullOrEmpty($UnsignedPackageOutputDirectory)) {
    if ($BuildEntryPoint -ne 'PackagedApplication') {
        throw 'Unsigned packaging requires the PackagedApplication build entry point.'
    }
    $packageOutputPath = [IO.Path]::GetFullPath($UnsignedPackageOutputDirectory)
    # Do not overwrite a previously qualified drop or mix old package files into
    # a new run. Native targets create this explicit new directory themselves.
    if (Test-Path -LiteralPath $packageOutputPath) {
        throw 'Unsigned package output directory must be fresh and absent.'
    }
}

$buildEntryPointRelativePath =
    if ($BuildEntryPoint -eq 'PackagedApplication') { 'src/JpgSpinner.App/JpgSpinner.App.vcxproj' }
    else { 'JpgSpinner.sln' }
$buildEntryPointPath = Join-Path $repositoryRoot $buildEntryPointRelativePath
if (-not (Test-Path -LiteralPath $buildEntryPointPath -PathType Leaf)) {
    throw "Native build entry point is absent: $buildEntryPointPath"
}
$repositoryNuGetConfigurationPath = Join-Path $repositoryRoot 'NuGet.config'
if (-not (Test-Path -LiteralPath $repositoryNuGetConfigurationPath -PathType Leaf)) {
    throw "Repository NuGet configuration is absent: $repositoryNuGetConfigurationPath"
}

$msBuildPlatformByArchitecture = @{
    x86 = 'Win32'
    x64 = 'x64'
    ARM64 = 'ARM64'
}
$msBuildPlatform = $msBuildPlatformByArchitecture[$Architecture]
$resolvedToolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') `
    -RepositoryRoot $repositoryRoot

$binaryLogDirectory = Join-Path $repositoryRoot (
    "artifacts\build-logs\$Architecture\$Configuration"
)
[void][System.IO.Directory]::CreateDirectory($binaryLogDirectory)
$binaryLogPath = Join-Path $binaryLogDirectory 'JpgSpinner.binlog'

# GitHub Actions and the other supported CI environments expose CI=true. Keep
# local builds explicit as false so an inherited MSBuild response/property file
# cannot silently switch dependency restore into a different mode.
$continuousIntegrationBuild = [string]::Equals(
    [System.Environment]::GetEnvironmentVariable('CI'),
    'true',
    [System.StringComparison]::OrdinalIgnoreCase
)
$continuousIntegrationBuildValue =
    if ($continuousIntegrationBuild) { 'true' } else { 'false' }

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $resolvedToolchain.msBuildExecutablePath
$startInfo.WorkingDirectory = $repositoryRoot
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$startInfo.RedirectStandardError = $true

foreach ($argument in @(
    $buildEntryPointPath,
    '-nologo',
    '-noAutoResponse',
    '-restore',
    # Bare -m uses every logical processor and exhausted commit memory on the
    # development host. Bound worker creation independently of CPU topology.
    # https://learn.microsoft.com/visualstudio/msbuild/msbuild-command-line-reference
    "-maxCpuCount:$MaximumBuildProcessCount",
    # A scripted build owns its worker lifetime. Do not retain MSBuild nodes
    # after exit: they keep directories open and prevent producer quiescence.
    # https://learn.microsoft.com/azure/devops/pipelines/troubleshooting/troubleshooting#msbuild-and-nodereusefalse
    '-nodeReuse:false',
    '-verbosity:minimal',
    "-p:Configuration=$Configuration",
    "-p:Platform=$msBuildPlatform",
    "-p:RestoreConfigFile=$repositoryNuGetConfigurationPath",
    '-p:RestoreLockedMode=true',
    "-p:ContinuousIntegrationBuild=$continuousIntegrationBuildValue",
    "/bl:$binaryLogPath"
)) {
    [void]$startInfo.ArgumentList.Add($argument)
}

if ($null -ne $packageOutputPath) {
    # Public single-project MSIX properties, not private targets or a hand-built
    # ZIP. Preserve project-owned toolchain/framework settings. This invocation
    # emits one architecture; bundling/signing/deployment are separate contracts.
    # https://learn.microsoft.com/windows/apps/windows-app-sdk/single-project-msix
    foreach ($argument in @('-p:GenerateAppxPackageOnBuild=true',
        "-p:AppxPackageDir=$packageOutputPath\", '-p:AppxBundle=Never',
        # CI requests the SDK's Release upload artifacts. The native targets
        # may also emit sideload/dependency working material during execution;
        # evaluated properties alone do not prove those files were omitted.
        # Inventory the actual drop and retain deployment prerequisites.
        '-p:UapAppxPackageBuildMode=CI', '-p:AppxPackageSigningEnabled=false')) {
        [void]$startInfo.ArgumentList.Add($argument)
    }
}

$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $startInfo
try {
    [void]$process.Start()
    $standardOutputReadTask = $process.StandardOutput.ReadToEndAsync()
    $standardErrorReadTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()

    $standardOutput = $standardOutputReadTask.GetAwaiter().GetResult()
    $standardError = $standardErrorReadTask.GetAwaiter().GetResult()
    if (-not [string]::IsNullOrWhiteSpace($standardOutput)) {
        [Console]::Out.Write($standardOutput)
    }
    if (-not [string]::IsNullOrWhiteSpace($standardError)) {
        [Console]::Error.Write($standardError)
    }

    $buildExitCode = $process.ExitCode
}
finally {
    $process.Dispose()
}

# Inspect the produced release image rather than accepting an option-shaped
# string in project XML. This also catches a later per-project LINK override.
# Debug builds retain their ordinary local debugging paths; this gate neither
# changes the actual symbol output nor asserts whole-payload reproducibility.
if ($buildExitCode -eq 0 -and $Configuration -ceq 'Release') {
    $applicationImagePath = Join-Path $repositoryRoot (
        "artifacts/bin/$msBuildPlatform/$Configuration/JpgSpinner.App/JpgSpinner.exe"
    )
    & (Join-Path $PSScriptRoot 'Test-PeProgramDatabaseReference.ps1') -ImagePath $applicationImagePath
}

exit $buildExitCode
