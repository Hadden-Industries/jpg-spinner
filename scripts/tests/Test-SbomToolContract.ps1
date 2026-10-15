<# Qualify the pinned native SDK adapter and independent SPDX validators.
   Inputs are maintained synthetic payload/component evidence, not an app bundle.
   All caches and generated output are explicit caller-owned locations. No tool
   installation, persistent environment change or JSON repair is performed. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GoExecutablePath,
    [Parameter(Mandatory)][string]$ModuleCacheDirectory,
    [Parameter(Mandatory)][string]$BuildCacheDirectory,
    [Parameter(Mandatory)][string]$OutputDirectory,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SchemaValidatorToolsDirectory,
    [Parameter(Mandatory)][string]$JavaExecutablePath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repositoryRoot 'scripts/JsonObjectMemberValidation.psm1') -Force
$pins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $repositoryRoot 'eng/sbom-tools.json') -Raw) -RequiredRootValueKind Object -SourceDescription 'SBOM tool pins' -AsHashtable
$expected = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/expected-contract.json') -Raw) -RequiredRootValueKind Object -SourceDescription 'Independent SBOM fixture contract' -AsHashtable

foreach ($inputFile in @(
    @{ Path=$SpdxSchemaPath; Hash=$pins.spdxSchema.sha256 },
    @{ Path=$SpdxToolsJarPath; Hash=$pins.spdxWorkgroupTools.jarSha256 }
)) {
    if ((Get-FileHash -LiteralPath $inputFile.Path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $inputFile.Hash) {
        throw "SPDX validator identity differs from the reviewed pin: $($inputFile.Path)"
    }
}
if (Test-Path -LiteralPath $OutputDirectory) { throw 'SBOM qualification output directory must be fresh and absent.' }
[void][IO.Directory]::CreateDirectory($OutputDirectory)
$manifestPath = Join-Path $OutputDirectory 'fixture.spdx.json'
$settings = @{
    GOMODCACHE=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ModuleCacheDirectory)
    GOCACHE=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BuildCacheDirectory)
    GOMAXPROCS='2'; GOTOOLCHAIN='local'; GOWORK='off'; GOENV='off'; GOFLAGS=''
    JPG_SPINNER_SBOM_QUALIFICATION_OUTPUT=$manifestPath
}
$previous = @{}
foreach ($name in $settings.Keys) { $previous[$name] = [Environment]::GetEnvironmentVariable($name, 'Process') }
Push-Location -LiteralPath (Join-Path $repositoryRoot 'eng/artifact-sbom')
try {
    foreach ($name in $settings.Keys) { [Environment]::SetEnvironmentVariable($name, $settings[$name], 'Process') }
    # Native module resolution plus go.sum owns dependency integrity. Never
    # replace its graph or silently upgrade modules to make the fixture pass.
    $moduleJson = & $GoExecutablePath list -mod=readonly -m -json $pins.syftSdk.module
    if ($LASTEXITCODE -ne 0) { throw 'Native pinned SDK module resolution failed.' }
    $sdk = ConvertFrom-JsonWithUniqueObjectMembers -JsonText ($moduleJson -join "`n") -RequiredRootValueKind Object -SourceDescription 'Native Syft module identity' -AsHashtable
    if ($sdk.Version -cne $pins.syftSdk.version -or $sdk.Sum -cne $pins.syftSdk.moduleSum -or $sdk.ContainsKey('Replace')) { throw 'Native Syft SDK identity differs from the reviewed module.' }
    & $GoExecutablePath test -mod=readonly -p=2 -count=1 -timeout=90s -v .
    if ($LASTEXITCODE -ne 0) { throw 'Native SBOM adapter contract tests failed.' }
}
finally {
    foreach ($name in $settings.Keys) { [Environment]::SetEnvironmentVariable($name, $previous[$name], 'Process') }
    Pop-Location
}
$payloadManifestPath = Join-Path $OutputDirectory 'payload.json'
& (Join-Path $repositoryRoot 'scripts/New-PayloadManifest.ps1') `
    -PayloadRoot (Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/payload') -OutputPath $payloadManifestPath | Out-Null
$payload = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $payloadManifestPath -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'Observed fixture payload' -AsHashtable
# Keep the independently fixed hashes as the fixture oracle. Merely comparing
# two generated inventories could let both consumers agree on the same mistake.
if ($payload.files.Count -ne $expected.payload.Count) { throw 'Fixture payload scope changed.' }
foreach ($file in $payload.files) {
    if (-not $expected.payload.Contains($file.relativePath) -or $file.sha256 -cne $expected.payload[$file.relativePath]) {
        throw 'Fixture payload differs from its independent fixed hash contract.'
    }
}
& (Join-Path $repositoryRoot 'scripts/Test-ArtifactSbom.ps1') -ManifestPath $manifestPath `
    -PayloadManifestPath $payloadManifestPath -ComponentContractPath (Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/expected-contract.json') `
    -PublicArtifactName 'JpgSpinner.OverlayQualification' -PublicArtifactVersion '2.0.0' `
    -SpdxSchemaPath $SpdxSchemaPath -SchemaValidatorToolsDirectory $SchemaValidatorToolsDirectory -JavaExecutablePath $JavaExecutablePath -SpdxToolsJarPath $SpdxToolsJarPath
Write-Output 'PASS: pinned native SDK, exact SPDX 3.0.1, payload SHA-256/scope, overlay and NuGet identities, membership, schema and semantic verification.'
