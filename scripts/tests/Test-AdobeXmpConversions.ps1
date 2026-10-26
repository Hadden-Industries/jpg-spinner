<# Qualify the real selected native SDK. Prepared source supplies its internal
   declarations; every implementation comes from the installed Adobe library. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$InstalledPrefix,
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'The conversion probe requires fresh output.' }
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$output = (Get-Item -LiteralPath $OutputDirectory).FullName
$source = (Get-Item -LiteralPath $SourceDirectory).FullName
$prefix = (Get-Item -LiteralPath $InstalledPrefix).FullName
$toolchain = & (Join-Path $root 'scripts/Resolve-MSBuildToolchain.ps1')
Import-Module (Join-Path $root 'scripts/ArtifactToolProcess.psm1')
[void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $root `
        -LogDirectory $output -LogName configure -Arguments @(
        '-S', (Join-Path $root 'eng/AdobeXmpConversionProbe'), '-B', (Join-Path $output 'build'),
        '-G', 'Visual Studio 18 2026', '-A', 'x64', '-T', "v145,host=x64,version=$($toolchain.vcToolsVersion)",
        '-DCMAKE_SYSTEM_VERSION=10.0.28000.0', "-DCMAKE_PREFIX_PATH=$prefix", "-DADOBE_XMP_SOURCE_DIRECTORY=$source"
    ))
[void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $root `
        -LogDirectory $output -LogName build -Arguments @('--build', (Join-Path $output 'build'), '--config', 'Release', '--parallel', '2'))
[void](Invoke-ArtifactToolProcess -Executable (Join-Path $output 'build/Release/AdobeXmpConversionProbe.exe') `
        -WorkingDirectory $output -LogDirectory $output -LogName conversions -Arguments @())
Write-Output 'PASS: installed Adobe integer and Unicode conversions qualify.'
