<# Prepare exact pre-certification Store-submission evidence, not a signed
   distribution artifact. Microsoft signs after certification. The caller must
   authenticate the actual clean-build provenance and exact selected container;
   a caller-provided hash alone does not authenticate a producer or publisher.
   This operation never submits, changes trust or declares certification. #>
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
New-JpgSpinnerBundleSbomEvidence @PSBoundParameters -ArtifactRole StoreSubmission
