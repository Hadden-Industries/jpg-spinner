<# Shared native bundle-evidence producer for two distinct distribution roles.
   The caller supplies frozen build-context evidence from the qualified native
   vcpkg exporter and resolved NuGet locks, plus its independently established
   required-component contract. Neither is inferred from installed binaries.
   This operation does not sign, install, resolve dependencies, certify linkage,
   prove a build execution or qualify deployment/runtime behaviour. The signed
   distribution role always verifies Windows trust before extraction. Store
   submission produces a different, explicitly pre-certification contract.
   Failed work stays diagnostic, never published.
   https://spdx.github.io/spdx-spec/v3.0.1/model/Software/Vocabularies/SbomType/ #>
function New-JpgSpinnerBundleSbomEvidence {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][ValidateSet('SignedDistribution','StoreSubmission',IgnoreCase=$false)][string]$ArtifactRole,
        [Parameter(Mandatory)][string]$BundlePath,
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{64}$')][string]$ExpectedBundleSha256,
        [Parameter(Mandatory)][string]$ComponentEvidenceRoot,
        [Parameter(Mandatory)][string]$ComponentContractPath,
        [Parameter(Mandatory)][string]$OutputDirectory,
        [Parameter(Mandatory)][string]$WorkingDirectory,
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
    $bundle=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($BundlePath)
    $components=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ComponentEvidenceRoot)
    $contract=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($ComponentContractPath)
    $output=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputDirectory)
    $working=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($WorkingDirectory)
    if (-not [IO.File]::Exists($bundle) -or [IO.Path]::GetExtension($bundle) -cne '.msixbundle') {
        throw 'Bundle SBOM requires an existing exact .msixbundle input.'
    }
    if (-not [IO.Directory]::Exists($components) -or -not [IO.File]::Exists($contract)) {
        throw 'Bundle SBOM build-component evidence or required contract is absent.'
    }
    if ((Test-Path -LiteralPath $output) -or (Test-Path -LiteralPath $working)) {
        throw 'Bundle SBOM output and working directories must be fresh and absent.'
    }
    foreach ($path in @($bundle,$components,$contract,$output,$working)) {
        $ancestor=$path
        while (-not [string]::IsNullOrEmpty($ancestor)) {
            if ((Test-Path -LiteralPath $ancestor) -and
                ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
                throw 'Bundle SBOM refuses reparse-point inputs or output ancestors.'
            }
            $ancestor=[IO.Path]::GetDirectoryName($ancestor)
        }
    }
    function Test-PathWithin([string]$Root,[string]$Path) {
        $relative=[IO.Path]::GetRelativePath($Root,$Path)
        return -not [IO.Path]::IsPathRooted($relative) -and $relative -ne '..' -and
            -not $relative.StartsWith("..$([IO.Path]::DirectorySeparatorChar)",[StringComparison]::Ordinal)
    }
    # Require disjoint inputs and destinations. Publication is a no-overwrite rename
    # of one complete directory on the same volume, not a series of visible files.
    foreach ($destination in @($working,$output)) {
        if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($destination))) {
            throw 'Bundle SBOM output parents must exist.'
        }
        if ((Test-PathWithin $components $destination) -or (Test-PathWithin $destination $components) -or
            (Test-PathWithin $destination $bundle) -or (Test-PathWithin $destination $contract)) {
            throw 'Bundle SBOM destinations must be disjoint from inventory inputs.'
        }
    }
    if ((Test-PathWithin $working $output) -or (Test-PathWithin $output $working) -or
        -not [string]::Equals([IO.Path]::GetPathRoot($working),[IO.Path]::GetPathRoot($output),[StringComparison]::OrdinalIgnoreCase)) {
        throw 'Bundle SBOM destinations must be disjoint and on the same volume.'
    }
    # Windows readers may share this handle, but writers/deleters cannot replace the
    # container between digest comparison, native trust verification and extraction.
    $bundleHandle=[IO.File]::OpenRead($bundle)
    try {
        $bundleHash=[Convert]::ToHexStringLower([Security.Cryptography.SHA256]::HashData($bundleHandle))
        if ($bundleHash -cne $ExpectedBundleSha256) {throw 'Bundle SBOM bundle digest differs from the selected artifact.'}
        [void][IO.Directory]::CreateDirectory($working)
        if ($ArtifactRole -ceq 'SignedDistribution') {
            # A signed-distribution request has no unsigned bypass. Preserve this
            # native trust-before-extraction boundary regardless of the Store route.
            $verified=& (Join-Path $PSScriptRoot 'Test-MsixSignature.ps1') -PackagePath $bundle -DiagnosticsPath (Join-Path $working 'signature.log')
            if ($verified.Sha256 -cne $bundleHash) {throw 'Native signature evidence differs from the selected bundle bytes.'}
        }
        # Store submission instead consumes the producer's exact selected unsigned
        # container. Its caller authenticates build provenance/platform attestation;
        # this hash/identity gate alone is not publisher or Windows-signature trust.
        $inspection=Join-Path $working 'inspection'
        & (Join-Path $PSScriptRoot 'Expand-JpgSpinnerBundle.ps1') -BundlePath $bundle -InspectionDirectory $inspection | Out-Null

        # The native manifest consumer has already checked the Store association,
        # closed architecture set and agreement of inner/outer package identities.
        $manifest=[Xml.XmlDocument]::new()
        $manifest.Load((Join-Path $inspection 'bundle/AppxMetadata/AppxBundleManifest.xml'))
        $name=$manifest.DocumentElement.Identity.Name
        $version=$manifest.DocumentElement.Identity.Version
        $candidate=Join-Path $working 'bundle-sbom-evidence'
        [void][IO.Directory]::CreateDirectory($candidate)
        $payload=Join-Path $inspection 'payloads'
        $componentManifest=Join-Path $candidate 'build-component-inputs.json'
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $components -OutputPath $componentManifest | Out-Null
        $contractCopy=Join-Path $candidate 'required-components.json'
        [IO.File]::Copy($contract,$contractCopy,$false)
        $sbom=Join-Path $candidate 'manifest.spdx.json'
        $generation=Join-Path $working 'generation'
        & (Join-Path $PSScriptRoot 'New-ArtifactSbom.ps1') -PayloadRoot $payload -ComponentEvidenceRoot $components `
            -ComponentContractPath $contractCopy -PublicArtifactName $name -PublicArtifactVersion $version `
            -OutputPath $sbom -WorkingDirectory $generation -GoExecutablePath $GoExecutablePath `
            -ModuleCacheDirectory $ModuleCacheDirectory -BuildCacheDirectory $BuildCacheDirectory `
            -SpdxSchemaPath $SpdxSchemaPath -SchemaValidatorToolsDirectory $SchemaValidatorToolsDirectory -JavaExecutablePath $JavaExecutablePath -SpdxToolsJarPath $SpdxToolsJarPath | Out-Null
        [IO.File]::Copy((Join-Path $generation 'payload.json'),(Join-Path $candidate 'payload.json'),$false)
        # Recheck quiescent inputs after cataloging. A changed component snapshot or
        # contract cannot inherit a passing result for different bytes.
        $after=Join-Path $working 'build-component-inputs-after.json'
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $components -OutputPath $after | Out-Null
        if ((Get-FileHash -LiteralPath $componentManifest).Hash -cne (Get-FileHash -LiteralPath $after).Hash -or
            (Get-FileHash -LiteralPath $contractCopy).Hash -cne (Get-FileHash -LiteralPath $contract).Hash) {
            throw 'Bundle SBOM component inputs changed during generation.'
        }
        $payloadAfter=Join-Path $working 'payload-after.json'
        & (Join-Path $PSScriptRoot 'New-PayloadManifest.ps1') -PayloadRoot $payload -OutputPath $payloadAfter | Out-Null
        if ((Get-FileHash -LiteralPath (Join-Path $candidate 'payload.json')).Hash -cne (Get-FileHash -LiteralPath $payloadAfter).Hash) {
            throw 'Bundle SBOM extracted payload changed during generation.'
        }
        # This is a separate evidence binding, not an edit to native SPDX output or
        # a build attestation. Build-context membership does not imply static linkage
        # or the complete deployed/instrumented runtime graph. Keep those gates open.
        $binding=[ordered]@{
            schemaVersion=1
            artifact=@{name=$name;version=$version;architectures=@('arm64','x64','x86')}
            inventoryScope=@{
                components='build-context'
                componentInputs='resolved-nuget-lock-and-installed-vcpkg-spdx'
                payload=$(if ($ArtifactRole -ceq 'SignedDistribution') {'exact-verified-bundle-payload'} else {'exact-submission-bundle-payload'})
                staticLinkage='not-inferred'
                deploymentRuntime='requires-separate-qualification'
                buildExecution='not-attested'
            }
            bundle=@{fileName=[IO.Path]::GetFileName($bundle);sha256=$bundleHash}
            evidence=@{}
        }
        if ($ArtifactRole -ceq 'StoreSubmission') {
            $binding['distribution']=@{channel='microsoft-store-submission';signature='store-managed-after-certification';certification='pending';releaseApproval='pending'}
        }
        foreach ($file in Get-ChildItem -LiteralPath $candidate -File) {
            $binding.evidence[$file.Name]=(Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        }
        $bindingJson=$binding | ConvertTo-Json -Depth 8
        $schemaName=if ($ArtifactRole -ceq 'SignedDistribution') {'release-sbom-binding.schema.json'} else {'store-submission-sbom-binding.schema.json'}
        $schema=Join-Path (Split-Path -Parent $PSScriptRoot) "eng/$schemaName"
        if (-not (Test-Json -Json $bindingJson -SchemaFile $schema)) {throw 'Bundle SBOM evidence binding has an unsupported shape.'}
        [IO.File]::WriteAllText((Join-Path $candidate 'binding.json'),$bindingJson,[Text.UTF8Encoding]::new($false))
        [IO.Directory]::Move($candidate,$output)
    }
    finally {$bundleHandle.Dispose()}
    Write-Output $output
}
Export-ModuleMember -Function New-JpgSpinnerBundleSbomEvidence
