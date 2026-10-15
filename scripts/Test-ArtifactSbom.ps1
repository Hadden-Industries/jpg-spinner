<# Validate native SPDX output against pinned official consumers and independent
   artifact evidence. Does not generate, repair, sign or publish an SBOM. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ManifestPath,
    [Parameter(Mandatory)][string]$PayloadManifestPath,
    [Parameter(Mandatory)][string]$ComponentContractPath,
    [Parameter(Mandatory)][string]$PublicArtifactName,
    [Parameter(Mandatory)][string]$PublicArtifactVersion,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$JavaExecutablePath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$pins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath (Join-Path $repositoryRoot 'eng/sbom-tools.json') -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'SBOM tool pins' -AsHashtable
foreach ($inputFile in @(
    @{Path=$SpdxSchemaPath;Hash=$pins.spdxSchema.sha256},
    @{Path=$SpdxToolsJarPath;Hash=$pins.spdxWorkgroupTools.jarSha256}
)) {
    if ((Get-FileHash -LiteralPath $inputFile.Path -Algorithm SHA256).Hash.ToLowerInvariant() -cne $inputFile.Hash) {
        throw 'SPDX validator identity differs from the reviewed pin.'
    }
}
$json = Get-Content -LiteralPath $ManifestPath -Raw
$document = ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -RequiredRootValueKind Object `
    -SourceDescription 'Native artifact SPDX output' -AsHashtable
if (-not (Test-Json -Json $json -SchemaFile $SpdxSchemaPath)) { throw 'Official SPDX 3.0.1 schema rejected native output.' }
$payloadManifest = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $PayloadManifestPath -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'Independent payload manifest' -AsHashtable
$componentContract = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (Get-Content -LiteralPath $ComponentContractPath -Raw) `
    -RequiredRootValueKind Object -SourceDescription 'Required component contract' -AsHashtable
if ($payloadManifest.schemaVersion -isnot [long] -or $payloadManifest.schemaVersion -ne 1 -or
    $payloadManifest.files -isnot [array] -or $payloadManifest.files.Count -eq 0 -or
    $componentContract.requiredPackages -isnot [array] -or $componentContract.requiredPackages.Count -eq 0) {
    throw 'Independent artifact evidence is empty or has an unsupported shape.'
}
$expectedFiles = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
foreach ($file in $payloadManifest.files) {
    if ($file.relativePath -isnot [string] -or [string]::IsNullOrEmpty($file.relativePath) -or
        $file.sha256 -isnot [string] -or -not $expectedFiles.TryAdd($file.relativePath,$file.sha256)) {
        throw 'Independent payload manifest contains an invalid or duplicate file identity.'
    }
}
# The platform parser and official schema own JSON/SPDX syntax. These residual
# checks bind that valid document to this artifact, not merely to a format.
if ($document['@context'] -cne 'https://spdx.org/rdf/3.0.1/spdx-context.jsonld') {
    throw 'Artifact SPDX context differs from 3.0.1.'
}
$nodes = @($document['@graph'])
$creation = @($nodes | Where-Object { $_.type -ceq 'CreationInfo' })
if ($creation.Count -eq 0 -or @($creation | Where-Object { $_.specVersion -cne '3.0.1' }).Count -ne 0) {
    throw 'Artifact SPDX specification version differs from 3.0.1.'
}
$documents = @($nodes | Where-Object { $_.type -ceq 'SpdxDocument' })
$sboms = @($nodes | Where-Object { $_.type -ceq 'software_Sbom' })
$rootPackages = @($nodes | Where-Object { $_.type -ceq 'software_Package' -and $_.name -ceq $PublicArtifactName -and $_.software_packageVersion -ceq $PublicArtifactVersion })
if ($documents.Count -ne 1 -or $documents[0].name -cne $PublicArtifactName -or $sboms.Count -ne 1 -or
    $rootPackages.Count -ne 1 -or $rootPackages[0].spdxId -cnotin $sboms[0].rootElement -or
    $rootPackages[0].spdxId -cnotin $sboms[0].element -or $sboms[0].spdxId -cnotin $documents[0].rootElement) {
    throw 'Artifact SPDX public identity or root membership differs from the contract.'
}
$files = @($nodes | Where-Object { $_.type -ceq 'software_File' })
$seenFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
if ($files.Count -ne $expectedFiles.Count) { throw 'Artifact SPDX payload differs from the independent manifest.' }
foreach ($file in $files) {
    $hashes = @($file.verifiedUsing)
    if (-not $expectedFiles.ContainsKey($file.name) -or -not $seenFiles.Add($file.name) -or
        $hashes.Count -ne 1 -or $hashes[0].algorithm -cne 'sha256' -or $hashes[0].hashValue -cne $expectedFiles[$file.name] -or
        $file.spdxId -cnotin $sboms[0].element) {
        throw 'Artifact SPDX payload differs from the independent manifest.'
    }
}
$seenComponents = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($identity in $componentContract.requiredPackages) {
    if ($identity.name -isnot [string] -or [string]::IsNullOrEmpty($identity.name) -or
        $identity.version -isnot [string] -or [string]::IsNullOrEmpty($identity.version)) {
        throw 'Required component contract contains an invalid package identity.'
    }
    # A native tuple encoding avoids conflating name/version strings containing
    # delimiters; this is identity bookkeeping, not another dependency resolver.
    $identityKey = ConvertTo-Json -InputObject @($identity.name,$identity.version) -Compress
    if (-not $seenComponents.Add($identityKey)) { throw 'Required component contract contains a duplicate package identity.' }
    $packages = @($nodes | Where-Object { $_.type -ceq 'software_Package' -and $_.name -ceq $identity.name -and $_.software_packageVersion -ceq $identity.version })
    if ($packages.Count -ne 1 -or $packages[0].spdxId -cnotin $sboms[0].element) {
        throw "Artifact SPDX required component is absent or ambiguous: $($identity.name)@$($identity.version)"
    }
}
# Use the official semantic verifier with bundled licence data, not a remote
# service. This qualified heap bounds schema initialization without hiding OOM.
& $JavaExecutablePath -Xmx1024m '-Dorg.spdx.useJARLicenseInfoOnly=true' -jar $SpdxToolsJarPath Verify $ManifestPath
if ($LASTEXITCODE -ne 0) { throw 'Official SPDX Workgroup semantic verification failed.' }
Write-Output 'PASS: artifact SPDX 3.0.1, public identity, exact payload hashes/membership, required components, schema and semantics.'
