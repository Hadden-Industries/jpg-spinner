<# Signed-distribution SBOM entry point. Always require native Windows bundle
   trust before extraction; there is deliberately no unsigned parameter. The
   separate Store-submission producer cannot emit this evidence contract. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedBundleSha256,
    [Parameter(Mandatory)][string]$ComponentEvidenceRoot,
    [Parameter(Mandatory)][string]$ComponentContractPath,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$BuildCacheDirectory,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SchemaValidatorToolsDirectory,
    [Parameter(Mandatory)][string]$JavaExecutablePath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSScriptRoot 'BundleSbomEvidence.psm1') -Force
New-JpgSpinnerBundleSbomEvidence @PSBoundParameters -ArtifactRole SignedDistribution
