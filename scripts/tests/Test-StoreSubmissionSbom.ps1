<# Positive pre-certification Store integration using a real unsigned bundle.
   This test does not claim native signature trust, certification, deployment,
   a clean application build or Store submission. Caller owns frozen inputs. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BundlePath,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedBundleSha256,
    [Parameter(Mandatory)][string]$ComponentEvidenceRoot,
    [Parameter(Mandatory)][string]$ComponentContractPath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
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
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('store-submission-sbom-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$parameters=@{}
foreach ($entry in $PSBoundParameters.GetEnumerator()) {
    if ($entry.Key -cne 'OwnedOutputRoot') {$parameters[$entry.Key]=$entry.Value}
}
$parameters.OutputDirectory=Join-Path $fixture 'published'
$parameters.WorkingDirectory=Join-Path $fixture 'work'
& (Join-Path $repositoryRoot 'scripts/New-StoreSubmissionSbom.ps1') @parameters | Out-Null
Import-Module (Join-Path $repositoryRoot 'scripts/JsonObjectMemberValidation.psm1') -Force
$bindingPath=Join-Path $parameters.OutputDirectory 'binding.json'
$binding=ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $bindingPath -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'Store submission evidence binding' -AsHashtable
if ($binding.distribution.channel -cne 'microsoft-store-submission' -or
    $binding.distribution.signature -cne 'store-managed-after-certification' -or
    $binding.distribution.certification -cne 'pending' -or $binding.distribution.releaseApproval -cne 'pending') {
    throw 'Store submission evidence was promoted to a signature/certification/release claim.'
}
if (-not (Test-Json -Json (Get-Content -LiteralPath $bindingPath -Raw) -SchemaFile (Join-Path $repositoryRoot 'eng/store-submission-sbom-binding.schema.json'))) {
    throw 'Store submission binding violates its separate closed schema.'
}
if ($binding.bundle.sha256 -cne $ExpectedBundleSha256 -or
    $binding.bundle.fileName -cne [IO.Path]::GetFileName($BundlePath)) {throw 'Published binding differs from the exact selected container.'}
$files=@(Get-ChildItem -LiteralPath $parameters.OutputDirectory -File)
if ($files.Count -ne 5 -or [IO.Directory]::Exists((Join-Path $parameters.WorkingDirectory 'bundle-sbom-evidence'))) {
    throw 'Store submission publication did not promote exactly one complete evidence directory.'
}
foreach ($entry in $binding.evidence.GetEnumerator()) {
    $hash=(Get-FileHash -LiteralPath (Join-Path $parameters.OutputDirectory $entry.Key) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -cne $entry.Value) {throw "Published evidence hash differs: $($entry.Key)"}
}
if ((Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ExpectedBundleSha256) {
    throw 'Store submission qualification changed the original container.'
}
Write-Output 'PASS: real pre-certification Store bundle produced exact-hash-bound native SBOM evidence without signature/release claims or input changes.'
