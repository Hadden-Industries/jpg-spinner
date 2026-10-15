<# Reject wrong bytes before extracting any Store submission or invoking Go.
   Positive native generation is a separate real-bundle qualification. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$UnsignedBundlePath,[Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('store-sbom-refusals-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$arguments=@{
    BundlePath=$UnsignedBundlePath;ExpectedBundleSha256=('0'*64)
    ComponentEvidenceRoot=(Join-Path $root tests/TestData/SbomContractFixture/source)
    ComponentContractPath=(Join-Path $root tests/TestData/SbomContractFixture/expected-contract.json)
    WorkingDirectory=(Join-Path $fixture work);OutputDirectory=(Join-Path $fixture output)
    GoExecutablePath=(Join-Path $fixture must-not-run-go.exe)
    ModuleCacheDirectory=(Join-Path $fixture unused-modules);BuildCacheDirectory=(Join-Path $fixture unused-build)
    SpdxSchemaPath=(Join-Path $fixture unused-schema);SchemaValidatorToolsDirectory=(Join-Path $fixture unused-validator)
    JavaExecutablePath=(Join-Path $fixture must-not-run-java.exe);SpdxToolsJarPath=(Join-Path $fixture unused.jar)
}
$message=$null
try {& (Join-Path $root scripts/New-StoreSubmissionSbom.ps1) @arguments | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('bundle digest differs',[StringComparison]::Ordinal) -or
    (Test-Path -LiteralPath $arguments.WorkingDirectory) -or (Test-Path -LiteralPath $arguments.OutputDirectory)) {
    throw "Wrong Store submission digest was not refused before effects: $message"
}
Write-Output 'PASS: Store submission digest mismatch refuses before extraction/cataloging/publication.'
