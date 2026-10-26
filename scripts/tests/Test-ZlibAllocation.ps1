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
Import-Module (Join-Path $repositoryRoot 'scripts/NativeDependencyProbe.psm1')
Invoke-NativeDependencyProbe -ProjectDirectory (Join-Path $repositoryRoot 'eng/ZlibAllocationProbe') `
    -ExecutableName ZlibAllocationProbe -InstalledPrefix $InstalledPrefix -OutputDirectory $OutputDirectory `
    -SourceDefinitions @{ ZLIB_SOURCE_DIRECTORY = $SourceDirectory }
Write-Output 'PASS: installed zlib allocator bounds and ordinary codec behaviour qualify.'
