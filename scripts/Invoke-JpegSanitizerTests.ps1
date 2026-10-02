[CmdletBinding()]
param(
    [string]$TestSpecification = '[jpeg][transform]',
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = [System.IO.Path]::GetFullPath($RepositoryRoot)
$toolchain = & (Join-Path $PSScriptRoot 'Resolve-MSBuildToolchain.ps1') -RepositoryRoot $repositoryRoot

function Invoke-RepositoryChildProcess {
    param([string]$Executable, [string[]]$Arguments, [int]$TimeoutSeconds, [switch]$WithSanitizerRuntime)
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $Executable
    $startInfo.WorkingDirectory = $repositoryRoot
    $startInfo.UseShellExecute = $false
    foreach ($argument in $Arguments) { [void]$startInfo.ArgumentList.Add($argument) }
    if ($WithSanitizerRuntime) {
        # Match the selected compiler's runtime, without changing user PATH or
        # copying DLLs into the application/package output directory.
        $startInfo.Environment['PATH'] = (Split-Path -Parent $toolchain.targetTools.x64.compilerExecutablePath) +
            [System.IO.Path]::PathSeparator + $startInfo.Environment['PATH']
    }
    $process = [System.Diagnostics.Process]::Start($startInfo)
    try {
        if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
            $process.Kill($true)
            $process.WaitForExit()
            throw "Sanitizer child process exceeded $TimeoutSeconds seconds: $Executable"
        }
        if ($process.ExitCode -ne 0) { throw "Sanitizer child process failed ($($process.ExitCode)): $Executable" }
    } finally {
        $process.Dispose()
    }
}

# Release is deliberate: this profile links a distinct, fully instrumented,
# release-only vcpkg graph. Mixing ordinary Debug libraries would violate the
# MSVC STL container-annotation ABI, not merely reduce sanitizer coverage.
Invoke-RepositoryChildProcess -Executable $toolchain.msBuildExecutablePath -TimeoutSeconds 1800 -Arguments @(
    (Join-Path $repositoryRoot 'tests/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.vcxproj'),
    '-nologo', '-noAutoResponse', '-p:Configuration=Release', '-p:Platform=x64',
    '-p:EnableASAN=true', '-p:LinkIncremental=false', '-verbosity:minimal'
)
Invoke-RepositoryChildProcess -Executable (
    Join-Path $repositoryRoot 'artifacts/bin/x64/Release/JpgSpinner.JpegTransformation.Tests/JpgSpinner.JpegTransformation.Tests.exe'
) -TimeoutSeconds 600 -WithSanitizerRuntime -Arguments @($TestSpecification)
