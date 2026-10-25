<# Qualify the actual dependency module without building the app. A fresh
   configure makes CMake execute its native test rather than reuse a cache.
   The caller supplies integrity-verified, prepared upstream source. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [ValidateSet('x86', 'x64', 'ARM64')][string]$Architecture = 'x64'
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = [IO.Path]::GetFullPath($SourceDirectory)
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'The Exiv2 filesystem probe requires fresh output.' }
if (-not [IO.File]::Exists((Join-Path $source 'cmake/FindFilesystem.cmake'))) {
    throw 'The actual Exiv2 filesystem module is absent.'
}
$toolchain = & (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1')
$platform = @{x86 = 'Win32'; x64 = 'x64'; ARM64 = 'ARM64' }[$Architecture]
Import-Module (Join-Path $root 'scripts/ArtifactToolProcess.psm1') -Force
[void][IO.Directory]::CreateDirectory($output)
$arguments = @(
    '-S', (Join-Path $root 'eng/Exiv2FilesystemProbe'), '-B', (Join-Path $output 'build'),
    '-G', 'Visual Studio 18 2026', '-A', $platform,
    '-T', "v145,host=x64,version=$($toolchain.vcToolsVersion)",
    '-DCMAKE_SYSTEM_VERSION=10.0.28000.0', "-DEXIV2_SOURCE_DIRECTORY=$source"
)
if ($Architecture -eq 'ARM64') { $arguments += '-DCMAKE_SYSTEM_NAME=Windows' }
[void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $root `
        -LogDirectory $output -LogName configure -Arguments $arguments)
$scope = if ($Architecture -eq 'ARM64') { 'cross-compilation/linking only' } else { 'native compile/link/run' }
Write-Output "PASS: actual Exiv2 filesystem module qualifies on $Architecture ($scope), with native format warnings as errors."
