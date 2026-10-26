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
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repositoryRoot 'scripts/NativeDependencyProbe.psm1')
Invoke-NativeDependencyProbe -ProjectDirectory (Join-Path $repositoryRoot 'eng/AdobeXmpConversionProbe') `
    -ExecutableName AdobeXmpConversionProbe -InstalledPrefix $InstalledPrefix -OutputDirectory $OutputDirectory `
    -SourceDefinitions @{ ADOBE_XMP_SOURCE_DIRECTORY = $SourceDirectory }
Write-Output 'PASS: installed Adobe integer and Unicode conversions qualify.'
