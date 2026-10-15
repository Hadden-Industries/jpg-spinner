<# Positive release integration requires a genuinely signed JPG Spinner bundle.
   Run only with protected-release authority and caller-frozen build evidence.
   The test does not manufacture credentials, trust a test signer, or bypass
   native verification when a release input is unavailable. #>
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
$fixture=Join-Path $OwnedOutputRoot ('release-sbom-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$parameters=@{}
foreach ($entry in $PSBoundParameters.GetEnumerator()) {
    if ($entry.Key -cne 'OwnedOutputRoot') {$parameters[$entry.Key]=$entry.Value}
}
$parameters.OutputDirectory=Join-Path $fixture 'published'
$parameters.WorkingDirectory=Join-Path $fixture 'work'
& (Join-Path $repositoryRoot 'scripts/Invoke-Sbom.ps1') @parameters | Out-Null
Import-Module (Join-Path $repositoryRoot 'scripts/JsonObjectMemberValidation.psm1') -Force
$bindingPath=Join-Path $parameters.OutputDirectory 'binding.json'
$binding=ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $bindingPath -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'Release evidence binding' -AsHashtable
if ($binding.bundle.sha256 -cne $ExpectedBundleSha256 -or
    $binding.bundle.fileName -cne [IO.Path]::GetFileName($BundlePath)) {throw 'Published binding differs from the exact selected container.'}
$files=@(Get-ChildItem -LiteralPath $parameters.OutputDirectory -File)
if ($files.Count -ne 5 -or [IO.Directory]::Exists((Join-Path $parameters.WorkingDirectory 'release-evidence'))) {
    throw 'Release publication did not promote exactly one complete evidence directory.'
}
foreach ($entry in $binding.evidence.GetEnumerator()) {
    $hash=(Get-FileHash -LiteralPath (Join-Path $parameters.OutputDirectory $entry.Key) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -cne $entry.Value) {throw "Published evidence hash differs: $($entry.Key)"}
}
if ((Get-FileHash -LiteralPath $BundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ExpectedBundleSha256) {
    throw 'Positive release qualification changed the signed container.'
}
Write-Output 'PASS: signed JPG Spinner bundle produced independently validated, exact-hash-bound build-context SBOM evidence without changing the original.'
