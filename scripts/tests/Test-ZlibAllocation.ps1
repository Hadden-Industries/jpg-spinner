<# Exercise the real selected native zlib allocator with bounded requests.
   Prepared source supplies declarations only; the installed archive supplies
   both allocator and public codec implementations. The caller owns output. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$InstalledPrefix,
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if (Test-Path -LiteralPath $OutputDirectory) { throw 'The allocator probe requires fresh output.' }
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$output = (Get-Item -LiteralPath $OutputDirectory).FullName
$source = (Get-Item -LiteralPath $SourceDirectory).FullName
$prefix = (Get-Item -LiteralPath $InstalledPrefix).FullName
$toolchain = & (Join-Path $repositoryRoot 'scripts/Resolve-MSBuildToolchain.ps1')
Import-Module (Join-Path $repositoryRoot 'scripts/ArtifactToolProcess.psm1')
[void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $repositoryRoot `
        -LogDirectory $output -LogName configure -Arguments @(
        '-S', (Join-Path $repositoryRoot 'eng/ZlibAllocationProbe'), '-B', (Join-Path $output 'build'),
        '-G', 'Visual Studio 18 2026', '-A', 'x64', '-T', "v145,host=x64,version=$($toolchain.vcToolsVersion)",
        '-DCMAKE_SYSTEM_VERSION=10.0.28000.0', "-DCMAKE_PREFIX_PATH=$prefix", "-DZLIB_SOURCE_DIRECTORY=$source"
    ))
[void](Invoke-ArtifactToolProcess -Executable $toolchain.cmakeExecutablePath -WorkingDirectory $repositoryRoot `
        -LogDirectory $output -LogName build -Arguments @('--build', (Join-Path $output 'build'), '--config', 'Release', '--parallel', '2'))
[void](Invoke-ArtifactToolProcess -Executable (Join-Path $output 'build/Release/ZlibAllocationProbe.exe') `
        -WorkingDirectory $output -LogDirectory $output -LogName allocations -Arguments @())
Write-Output 'PASS: installed zlib allocator bounds and ordinary codec behaviour qualify.'
