[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration,

    [Parameter(Mandatory)]
    [ValidateSet('x86', 'x64', 'ARM64')]
    [string]$Architecture,

    [Parameter()]
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
if (-not (Test-Path -LiteralPath $repositoryRoot -PathType Container)) {
    throw "RepositoryRoot does not identify an existing directory: $repositoryRoot"
}

$solutionPath = Join-Path $repositoryRoot 'JpgSpinner.sln'
if (-not (Test-Path -LiteralPath $solutionPath -PathType Leaf)) {
    throw "Modern solution is absent: $solutionPath"
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
    $solutionPath,
    '-nologo',
    '-noAutoResponse',
    '-restore',
    '-m',
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

exit $buildExitCode
