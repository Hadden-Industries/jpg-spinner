<# Real native generation must not publish a candidate whose independently
   required dependency version is wrong. Inputs and process settings survive. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$BuildCacheDirectory,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$JavaExecutablePath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $OwnedOutputRoot ('artifact-generation-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$contractPath = Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/expected-contract.json'
$wrongContract = Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json -AsHashtable
$wrongContract.requiredPackages[0].version = '0.0.0'
$wrongContractPath = Join-Path $fixture 'wrong-components.json'
$wrongContract | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $wrongContractPath -Encoding utf8NoBOM
$arguments = @{
    PayloadRoot=(Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/payload')
    ComponentEvidenceRoot=(Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/source')
    PublicArtifactName='JpgSpinner.OverlayQualification'; PublicArtifactVersion='2.0.0'
    GoExecutablePath=$GoExecutablePath;ModuleCacheDirectory=$ModuleCacheDirectory;BuildCacheDirectory=$BuildCacheDirectory
    SpdxSchemaPath=$SpdxSchemaPath;JavaExecutablePath=$JavaExecutablePath;SpdxToolsJarPath=$SpdxToolsJarPath
}
$inputHashes = @{}
foreach ($directory in @($arguments.PayloadRoot,$arguments.ComponentEvidenceRoot)) {
    foreach ($file in Get-ChildItem -LiteralPath $directory -File -Recurse) {
        $inputHashes[$file.FullName] = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash
    }
}
$inputHashes[$contractPath] = (Get-FileHash -LiteralPath $contractPath -Algorithm SHA256).Hash
$environmentBefore = @{}
foreach ($name in @('GOMODCACHE','GOCACHE','GOMAXPROCS','GOTOOLCHAIN','GOWORK','GOENV','GOFLAGS','GOPROXY')) {
    $environmentBefore[$name] = [Environment]::GetEnvironmentVariable($name,'Process')
}
function Assert-GenerationPreservation {
    # Check both refusal and success in the calling host, not just a child whose
    # inherited environment disappears when it exits.
    foreach ($path in $inputHashes.Keys) {
        if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $inputHashes[$path]) { throw "Inventory input changed: $path" }
    }
    foreach ($name in $environmentBefore.Keys) {
        if ([Environment]::GetEnvironmentVariable($name,'Process') -cne $environmentBefore[$name]) { throw "Process setting was not restored: $name" }
    }
}
$output = Join-Path $fixture 'must-not-publish.spdx.json'
$working = Join-Path $fixture 'refused-candidate'
$message = $null
try {
    & (Join-Path $repositoryRoot 'scripts/New-ArtifactSbom.ps1') @arguments -ComponentContractPath $wrongContractPath -OutputPath $output -WorkingDirectory $working
}
catch { $message = $_.Exception.Message }
Assert-GenerationPreservation
if ($null -eq $message -or -not $message.Contains('required component is absent',[StringComparison]::Ordinal) -or [IO.File]::Exists($output)) {
    throw "Native generation published or failed to reject a wrong required dependency: $message"
}
if (-not [IO.File]::Exists((Join-Path $working 'candidate.spdx.json'))) { throw 'Refused native candidate was not preserved for diagnosis.' }
$acceptedOutput = Join-Path $fixture 'accepted.spdx.json'
& (Join-Path $repositoryRoot 'scripts/New-ArtifactSbom.ps1') @arguments -ComponentContractPath $contractPath -OutputPath $acceptedOutput -WorkingDirectory (Join-Path $fixture 'accepted-candidate')
Assert-GenerationPreservation
if (-not [IO.File]::Exists($acceptedOutput)) { throw 'Qualified native fixture was not published.' }
Write-Output 'PASS: native SBOM generation publishes only after independent dependency, payload, schema and semantic checks.'
