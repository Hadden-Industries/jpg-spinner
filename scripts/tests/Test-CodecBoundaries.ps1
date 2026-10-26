<# Characterize the installed codecs' native row/scale and Adam7 invariants.
   This is positive behavioral evidence, not a claim that a warning is fixed. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$InstalledPrefix,
    [Parameter(Mandatory)][string]$OutputDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repositoryRoot 'scripts/NativeDependencyProbe.psm1')
Invoke-NativeDependencyProbe -ProjectDirectory (Join-Path $repositoryRoot 'eng/CodecBoundaryProbe') `
    -ExecutableName CodecBoundaryProbe -InstalledPrefix $InstalledPrefix -OutputDirectory $OutputDirectory
Write-Output 'PASS: installed native JPEG scale and PNG interlace controls.'
