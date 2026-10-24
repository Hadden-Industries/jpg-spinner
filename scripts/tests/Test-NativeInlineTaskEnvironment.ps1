<# Exercise original Microsoft tasks after native C++ environment selection.
   Builds no application code, restores no packages and changes no parent
   environment. Native MSBuild, Roslyn and the common process boundary own
   execution, diagnostics, timeouts and argument quoting. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture = 'x64',
    [ValidateSet('Debug', 'Release', 'Fuzz')][string]$Configuration = 'Release',
    [ValidateSet('JpgSpinner.Domain', 'JpgSpinner.JpegTransformation.Tests',
        'JpgSpinner.FuzzToolchain.Smoke', 'JpgSpinner.JpegSegmentScanner.Fuzz')]
    [string]$Project = 'JpgSpinner.Domain',
    [switch]$EnableASAN
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$isFuzzProject = $Project -in @('JpgSpinner.FuzzToolchain.Smoke', 'JpgSpinner.JpegSegmentScanner.Fuzz')
if (($isFuzzProject -and ($Architecture -ne 'x64' -or $Configuration -ne 'Fuzz')) -or
    (-not $isFuzzProject -and $Configuration -eq 'Fuzz') -or
    ($EnableASAN -and ($Project -ne 'JpgSpinner.JpegTransformation.Tests' -or $Architecture -ne 'x64'))) {
    throw 'Native environment selection does not name a supported project configuration.'
}
$projectPath = if ($Project -eq 'JpgSpinner.Domain') {
    Join-Path $repositoryRoot 'src/JpgSpinner.Domain/JpgSpinner.Domain.vcxproj'
} elseif ($Project -eq 'JpgSpinner.JpegTransformation.Tests') {
    Join-Path $repositoryRoot 'tests/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.vcxproj'
} else { Join-Path $repositoryRoot "fuzz/$Project/$Project.vcxproj" }
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Native environment qualification requires fresh output.' }
[void][IO.Directory]::CreateDirectory($output)
$toolchain = & (Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1')
Import-Module (Join-Path $repositoryRoot 'scripts/ArtifactToolProcess.psm1') -Force
$platform = if ($Architecture -eq 'x86') { 'Win32' } else { $Architecture }
$fixture = Join-Path $PSScriptRoot 'Fixtures/NativeInlineTaskEnvironmentProbe.targets'
$arguments = @(
    $projectPath,
    '-nologo', '-noAutoResponse', '-nodeReuse:false', '-maxCpuCount:1',
    '-target:ProbeNativeInlineTaskEnvironment', "-p:Platform=$platform",
    "-p:Configuration=$Configuration", "-p:CustomAfterDirectoryBuildTargets=$fixture",
    "-p:ProbeNativeLibraryInventoryPath=$(Join-Path $output 'native-library-directories.txt')",
    # Keep compiler diagnostics visible and fail this qualification on any one.
    # This affects the diagnostic invocation, not the app's compilation policy.
    '-warnAsError', '-verbosity:minimal'
)
if ($EnableASAN) { $arguments += '-p:EnableASAN=true' }
# Use the shared qualification ceiling for cold MSBuild/Roslyn
# initialization. These controls assert native completion and warning/refusal
# behavior, not a thirty-second performance requirement.
$positive = Invoke-ArtifactToolProcess -Executable $toolchain.msBuildExecutablePath `
    -Arguments ($arguments + "-bl:$(Join-Path $output 'native-positive.binlog')") `
    -WorkingDirectory $repositoryRoot -LogDirectory $output -LogName native-positive
if (-not $positive.Contains("Native inline-task environment qualified: $platform / $Configuration", [StringComparison]::Ordinal)) {
    throw 'The original native tasks did not reach their completion assertion.'
}
if ($EnableASAN -or $isFuzzProject) {
    # The instrumented projects append the ordinary MSVC directory for ASan
    # and libFuzzer. It must survive property import ordering, after Spectre.
    $directories = [IO.File]::ReadAllLines((Join-Path $output 'native-library-directories.txt'))
    $ordinaryDirectory = Join-Path $toolchain.vcToolsDirectoryPath 'lib/x64'
    $spectreDirectory = Join-Path $toolchain.vcToolsDirectoryPath 'lib/spectre/x64'
    $ordinaryIndex = -1
    $spectreIndex = -1
    for ($index = 0; $index -lt $directories.Count; ++$index) {
        $directory = [IO.Path]::GetFullPath($directories[$index])
        if ($directory.Equals($ordinaryDirectory, [StringComparison]::OrdinalIgnoreCase) -and $ordinaryIndex -lt 0) { $ordinaryIndex = $index }
        if ($directory.Equals($spectreDirectory, [StringComparison]::OrdinalIgnoreCase) -and $spectreIndex -lt 0) { $spectreIndex = $index }
    }
    if ($spectreIndex -lt 0 -or $ordinaryIndex -lt 0 -or $spectreIndex -ge $ordinaryIndex) {
        throw 'Instrumented projects must retain the ordinary ASan/libFuzzer library directory after Spectre.'
    }
}

# Reject missing paths and ordinary files before native environment publication.
# Never repair an installation or hide a missing mandatory library directory.
$ordinaryFile = Join-Path $output 'not-a-library-directory.txt'
[IO.File]::WriteAllText($ordinaryFile, 'Owned negative-control sentinel')
foreach ($invalidPath in @((Join-Path $output 'absent-library-directory'), $ordinaryFile)) {
    $name = if ($invalidPath -eq $ordinaryFile) { 'file' } else { 'absent' }
    $rejected = $false
    try {
        [void](Invoke-ArtifactToolProcess -Executable $toolchain.msBuildExecutablePath `
            -Arguments ($arguments + "-p:LibraryPath=$invalidPath") `
            -WorkingDirectory $repositoryRoot -LogDirectory $output -LogName "native-$name")
    }
    catch {
        $diagnostic = [IO.File]::ReadAllText((Join-Path $output "native-$name.stdout.log"))
        if (-not $diagnostic.Contains('Required native library directory is absent or not a directory:', [StringComparison]::Ordinal)) {
            throw
        }
        $rejected = $true
    }
    if (-not $rejected) { throw "Native library directory validation accepted $name input." }
}
if ([IO.File]::ReadAllText($ordinaryFile) -cne 'Owned negative-control sentinel') {
    throw 'Native refusal changed the existing sentinel.'
}
Write-Output "PASS: $Project $Architecture $Configuration native inline tasks are warning-free; invalid library directories refuse before environment publication."
