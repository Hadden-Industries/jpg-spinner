<# The sidecar describes build-context evidence, not runtime composition. Use
   the platform JSON Schema consumer to exercise the closed contract directly;
   these synthetic hashes are schema fixtures, not a signed-release receipt. #>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$schema=Join-Path $repositoryRoot 'eng/release-sbom-binding.schema.json'
$fixture=@{
    schemaVersion=1
    artifact=@{name='HaddenIndustriesLtd.JPGSpinner';version='2.0.0.0';architectures=@('arm64','x64','x86')}
    inventoryScope=@{
        components='build-context';componentInputs='resolved-nuget-lock-and-installed-vcpkg-spdx'
        payload='exact-verified-bundle-payload';staticLinkage='not-inferred'
        deploymentRuntime='requires-separate-qualification';buildExecution='not-attested'
    }
    bundle=@{fileName='JpgSpinner.msixbundle';sha256=('a'*64)}
    evidence=@{
        'manifest.spdx.json'=('b'*64);'payload.json'=('c'*64)
        'build-component-inputs.json'=('d'*64);'required-components.json'=('e'*64)
    }
}
$json=$fixture | ConvertTo-Json -Depth 8
if (-not (Test-Json -Json $json -SchemaFile $schema)) {throw 'Explicit build-context binding was rejected.'}
foreach ($case in @(
    @{Name='runtime-component-claim';Mutate={param($binding) $binding.inventoryScope.components='runtime'}},
    @{Name='implied-linkage';Mutate={param($binding) $binding.inventoryScope.staticLinkage='verified'}},
    @{Name='implied-deployment';Mutate={param($binding) $binding.inventoryScope.deploymentRuntime='verified'}},
    @{Name='implied-build';Mutate={param($binding) $binding.inventoryScope.buildExecution='attested'}},
    @{Name='missing-scope';Mutate={param($binding) $binding.Remove('inventoryScope')}},
    @{Name='missing-component-contract';Mutate={param($binding) $binding.evidence.Remove('required-components.json')}},
    @{Name='non-string-hash';Mutate={param($binding) $binding.bundle.sha256=42}},
    @{Name='private-path';Mutate={param($binding) $binding.bundle.fileName='C:\private\JpgSpinner.msixbundle'}},
    @{Name='missing-architecture';Mutate={param($binding) $binding.artifact.architectures=@('x64','x86')}},
    @{Name='unsupported-field';Mutate={param($binding) $binding.releaseApproved=$true}}
)) {
    $candidate=$json | ConvertFrom-Json -AsHashtable
    & $case.Mutate $candidate | Out-Null
    $candidateJson=$candidate | ConvertTo-Json -Depth 8
    # Invalid JSON Schema instances emit expected errors as well as false.
    # Preserve the actual Boolean decision; no exception/setup failure passes.
    if (Test-Json -Json $candidateJson -SchemaFile $schema -ErrorAction SilentlyContinue) {
        throw "Release evidence schema accepted '$($case.Name)'."
    }
}
Write-Output 'PASS: native JSON Schema enforces explicit build-context scope, exact binding shape/hashes and independent runtime/build qualification.'
