<# Store submission may not be mistaken for signed distribution, certification
   or release approval. Native schema validation owns JSON grammar/types. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$schema=Join-Path $root eng/store-submission-sbom-binding.schema.json
# Native PowerShell AST verifies that the public entry points select fixed
# roles rather than exposing a caller-controlled signature-bypass parameter.
function Assert-FixedArtifactRole([string]$Text,[string]$Role) {
    $tokens=$null;$errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseInput($Text,[ref]$tokens,[ref]$errors)
    if ($errors.Count -ne 0) {throw 'Native artifact-role script parse failed.'}
    if ($ast.ParamBlock.Parameters.Name.VariablePath.UserPath -ccontains 'ArtifactRole') {throw 'Public entry point exposes a caller-selected artifact role.'}
    $commands=@($ast.FindAll({param($node) $node -is [Management.Automation.Language.CommandAst] -and $node.GetCommandName() -ceq 'New-JpgSpinnerBundleSbomEvidence'},$true))
    if ($commands.Count -ne 1) {throw 'Public entry point must delegate once to the native bundle-evidence producer.'}
    $elements=$commands[0].CommandElements
    $roles=@(for ($index=0;$index -lt ($elements.Count-1);$index++) {
        if ($elements[$index] -is [Management.Automation.Language.CommandParameterAst] -and $elements[$index].ParameterName -ceq 'ArtifactRole') {
            $value=$elements[$index+1]
            if ($value -is [Management.Automation.Language.StringConstantExpressionAst]) {$value.Value}
        }
    })
    if ($roles.Count -ne 1 -or $roles[0] -cne $Role) {throw 'Public entry point changed its fixed artifact role.'}
}
$signedWrapper=[IO.File]::ReadAllText((Join-Path $root scripts/Invoke-Sbom.ps1))
Assert-FixedArtifactRole $signedWrapper 'SignedDistribution'
Assert-FixedArtifactRole ([IO.File]::ReadAllText((Join-Path $root scripts/New-StoreSubmissionSbom.ps1))) 'StoreSubmission'
$message=$null
try {Assert-FixedArtifactRole ($signedWrapper.Replace('-ArtifactRole SignedDistribution','-ArtifactRole StoreSubmission')) 'SignedDistribution'}
catch {$message=$_.Exception.Message}
if ($message -cne 'Public entry point changed its fixed artifact role.') {throw 'A valid-but-wrong signed entry-point role was not refused.'}
$fixture=@{
    schemaVersion=1
    artifact=@{name='HaddenIndustriesLtd.JPGSpinner';version='2.0.0.0';architectures=@('arm64','x64','x86')}
    inventoryScope=@{components='build-context';componentInputs='resolved-nuget-lock-and-installed-vcpkg-spdx';payload='exact-submission-bundle-payload';staticLinkage='not-inferred';deploymentRuntime='requires-separate-qualification';buildExecution='not-attested'}
    bundle=@{fileName='JpgSpinner.msixbundle';sha256=('1'*64)}
    evidence=@{'manifest.spdx.json'=('2'*64);'payload.json'=('3'*64);'build-component-inputs.json'=('4'*64);'required-components.json'=('5'*64)}
    distribution=@{channel='microsoft-store-submission';signature='store-managed-after-certification';certification='pending';releaseApproval='pending'}
}
$json=$fixture | ConvertTo-Json -Depth 8
if (-not (Test-Json -Json $json -SchemaFile $schema)) {throw 'Valid Store-only binding was rejected.'}
if (Test-Json -Json $json -SchemaFile (Join-Path $root eng/release-sbom-binding.schema.json) -ErrorAction SilentlyContinue) {throw 'Store submission was admitted as signed-distribution evidence.'}
$signed=$json | ConvertFrom-Json -AsHashtable
$signed.Remove('distribution')
$signed.inventoryScope.payload='exact-verified-bundle-payload'
$signedJson=$signed | ConvertTo-Json -Depth 8
if (-not (Test-Json -Json $signedJson -SchemaFile (Join-Path $root eng/release-sbom-binding.schema.json)) -or
    (Test-Json -Json $signedJson -SchemaFile $schema -ErrorAction SilentlyContinue)) {throw 'The two artifact roles are not mutually exclusive.'}
foreach ($case in @(
    @{Name='certified';Mutate={param($value) $value.distribution.certification='passed'}},
    @{Name='approved';Mutate={param($value) $value.distribution.releaseApproval='approved'}},
    @{Name='trusted-signature';Mutate={param($value) $value.distribution.signature='verified'}},
    @{Name='wrong-channel';Mutate={param($value) $value.distribution.channel='sideload'}},
    @{Name='missing-distribution';Mutate={param($value) $value.Remove('distribution')}},
    @{Name='runtime-claim';Mutate={param($value) $value.inventoryScope.components='runtime'}},
    @{Name='missing-component-contract';Mutate={param($value) $value.evidence.Remove('required-components.json')}},
    @{Name='wrong-hash-type';Mutate={param($value) $value.bundle.sha256=1}},
    @{Name='unknown-field';Mutate={param($value) $value.approved=$true}}
)) {
    $candidate=$json | ConvertFrom-Json -AsHashtable
    & $case.Mutate $candidate | Out-Null
    if (Test-Json -Json ($candidate | ConvertTo-Json -Depth 8) -SchemaFile $schema -ErrorAction SilentlyContinue) {throw "Store binding admitted $($case.Name)."}
    Write-Output "PASS Store binding refusal: $($case.Name)"
}
