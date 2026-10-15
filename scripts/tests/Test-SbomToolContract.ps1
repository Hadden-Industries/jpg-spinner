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
$json = Get-Content -LiteralPath $manifestPath -Raw
$document = ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -RequiredRootValueKind Object -SourceDescription 'Native SPDX fixture output' -AsHashtable
if (-not (Test-Json -Json $json -SchemaFile $SpdxSchemaPath)) { throw 'Official SPDX 3.0.1 schema rejected the unmodified native output.' }
if ($document['@context'] -cne $expected.context) { throw 'Wrong native SPDX context.' }
$nodes = @($document['@graph'])
$creation = @($nodes | Where-Object { $_.type -ceq 'CreationInfo' })
if ($creation.Count -eq 0 -or @($creation | Where-Object { $_.specVersion -cne $expected.specVersion }).Count -ne 0) { throw 'Wrong native SPDX specification version.' }
$publicDocument = @($nodes | Where-Object { $_.type -ceq 'SpdxDocument' })
if ($publicDocument.Count -ne 1 -or $publicDocument[0].name -cne 'JpgSpinner.OverlayQualification') { throw 'Public artifact identity is missing or ambiguous.' }
$files = @($nodes | Where-Object { $_.type -ceq 'software_File' })
$sbom = @($nodes | Where-Object { $_.type -ceq 'software_Sbom' })
if ($files.Count -ne $expected.payload.Count -or $sbom.Count -ne 1) { throw 'Native SPDX payload scope differs from the independent contract.' }
foreach ($file in $files) {
    $hashes = @($file.verifiedUsing)
    if (-not $expected.payload.Contains($file.name) -or $hashes.Count -ne 1 -or $hashes[0].algorithm -cne 'sha256' -or $hashes[0].hashValue -cne $expected.payload[$file.name] -or $file.spdxId -cnotin $sbom[0].element) { throw 'Native SPDX payload identity, digest or membership differs from the reviewed fixture.' }
}
foreach ($identity in $expected.requiredPackages) {
    $packages = @($nodes | Where-Object { $_.type -ceq 'software_Package' -and $_.name -ceq $identity.name -and $_.software_packageVersion -ceq $identity.version })
    if ($packages.Count -ne 1 -or $packages[0].spdxId -cnotin $sbom[0].element) { throw "Native SPDX lost or ambiguously identifies $($identity.name)@$($identity.version)." }
}
# The official verifier resolves its bundled licence data, not remote licence
# services. Its schema initialization requires the qualified bounded 1 GiB heap.
& $JavaExecutablePath -Xmx1024m '-Dorg.spdx.useJARLicenseInfoOnly=true' -jar $SpdxToolsJarPath Verify $manifestPath
if ($LASTEXITCODE -ne 0) { throw 'Official SPDX Workgroup semantic verification failed.' }
Write-Output 'PASS: pinned native SDK, exact SPDX 3.0.1, payload SHA-256/scope, overlay and NuGet identities, membership, schema and semantic verification.'
