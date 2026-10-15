<# Mutate a retained native document, not its generator. Each refusal must come
   from the artifact-evidence gate rather than missing tools or invalid JSON. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$QualifiedManifestPath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$JavaExecutablePath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $OwnedOutputRoot ('artifact-sbom-controls-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$payloadManifest = Join-Path $fixture 'payload.json'
& (Join-Path $repositoryRoot 'scripts/New-PayloadManifest.ps1') `
    -PayloadRoot (Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/payload') -OutputPath $payloadManifest | Out-Null
$arguments = @{
    PayloadManifestPath=$payloadManifest
    ComponentContractPath=(Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/expected-contract.json')
    PublicArtifactName='JpgSpinner.OverlayQualification'
    PublicArtifactVersion='2.0.0'
    SpdxSchemaPath=$SpdxSchemaPath; JavaExecutablePath=$JavaExecutablePath; SpdxToolsJarPath=$SpdxToolsJarPath
}
$producer = Join-Path $repositoryRoot 'scripts/Test-ArtifactSbom.ps1'
$before = (Get-FileHash -LiteralPath $QualifiedManifestPath -Algorithm SHA256).Hash
$original = Get-Content -LiteralPath $QualifiedManifestPath -Raw | ConvertFrom-Json -AsHashtable
$fileNodes = @($original['@graph'] | Where-Object { $_.type -ceq 'software_File' })
if ($fileNodes.Count -ne 1 -or $fileNodes[0].name -cne 'fixture.txt' -or
    $fileNodes[0].verifiedUsing[0].hashValue -cne '634073ab8f2a89bf749373045683c28da783014554321b2de4ea222f3ac23cce') {
    throw 'Retained native fixture does not contain the independent payload/hash precondition.'
}
foreach ($case in @(
    @{Name='wrong-hash';Diagnostic='payload differs';Mutate={ param($doc)
        $file = @($doc['@graph'] | Where-Object { $_.type -ceq 'software_File' })[0]
        $file.verifiedUsing[0].hashValue = '0000000000000000000000000000000000000000000000000000000000000000'
    }},
    @{Name='missing-file-membership';Diagnostic='payload differs';Mutate={ param($doc)
        $file = @($doc['@graph'] | Where-Object { $_.type -ceq 'software_File' })[0]
        $sbom = @($doc['@graph'] | Where-Object { $_.type -ceq 'software_Sbom' })[0]
        $sbom.element = @($sbom.element | Where-Object { $_ -cne $file.spdxId })
    }},
    @{Name='wrong-component-version';Diagnostic='required component is absent';Mutate={ param($doc)
        $component = @($doc['@graph'] | Where-Object { $_.type -ceq 'software_Package' -and $_.name -ceq 'exiv2' -and $_.software_packageVersion -ceq '0.28.9#1' })
        if ($component.Count -ne 1) { throw 'Required component mutation precondition is absent.' }
        $component[0].software_packageVersion = '0.28.8'
    }},
    @{Name='wrong-public-version';Diagnostic='public identity or root membership';Mutate={ param($doc)
        $component = @($doc['@graph'] | Where-Object { $_.type -ceq 'software_Package' -and $_.name -ceq 'JpgSpinner.OverlayQualification' })[0]
        $component.software_packageVersion = '9.0.0'
    }}
)) {
    $mutated = Get-Content -LiteralPath $QualifiedManifestPath -Raw | ConvertFrom-Json -AsHashtable
    & $case.Mutate $mutated
    $candidate = Join-Path $fixture "$($case.Name).spdx.json"
    # Native JSON serialization constructs deliberately invalid test artifacts;
    # production output is never rewritten to make its contract pass.
    $mutated | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $candidate -Encoding utf8NoBOM
    $message = $null
    try { & $producer -ManifestPath $candidate @arguments }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal)) {
        throw "Mutation '$($case.Name)' was not refused by its artifact gate: $message"
    }
}

# Duplicate members must be refused before PowerShell's lossy conversion. This
# is a malformed test document, not a new JSON parser or output-repair step.
$duplicate = Join-Path $fixture 'duplicate-context.spdx.json'
$originalJson = Get-Content -LiteralPath $QualifiedManifestPath -Raw
if ($originalJson[0] -cne '{') { throw 'Native fixture object-root precondition is absent.' }
$originalJson.Insert(1,'"@context":"https://spdx.org/rdf/3.0.1/spdx-context.jsonld",') | Set-Content -LiteralPath $duplicate -Encoding utf8NoBOM
$message = $null
try { & $producer -ManifestPath $duplicate @arguments } catch { $message = $_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('not valid strict JSON',[StringComparison]::Ordinal)) {
    throw "Duplicate-member rejection before conversion is absent: $message"
}
& $producer -ManifestPath $QualifiedManifestPath @arguments
if ((Get-FileHash -LiteralPath $QualifiedManifestPath -Algorithm SHA256).Hash -cne $before) { throw 'Artifact validation changed its native input.' }
Write-Output 'PASS: artifact validator refuses wrong hashes/membership/component/public versions and duplicate JSON members, accepting the unchanged native fixture.'
