<# Exercise the release entry point with a real unsigned JPG Spinner bundle.
   Native trust must fail before extraction, Go invocation or publication. No
   fake signed package, validator stub, certificate or trust-store edit is used. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$UnsignedBundlePath,
    [Parameter(Mandatory)][string]$OwnedOutputRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture=Join-Path $OwnedOutputRoot ('release-sbom-controls-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$hash=(Get-FileHash -LiteralPath $UnsignedBundlePath -Algorithm SHA256).Hash.ToLowerInvariant()
$arguments=@{
    BundlePath=$UnsignedBundlePath;ExpectedBundleSha256=$hash
    ComponentEvidenceRoot=(Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/source')
    ComponentContractPath=(Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/expected-contract.json')
    OutputDirectory=(Join-Path $fixture 'must-not-publish')
    WorkingDirectory=(Join-Path $fixture 'unsigned-work')
    # These absent downstream tools are intentional sentinels. The test accepts
    # only native SignTool's precise refusal; a setup failure cannot pass it.
    GoExecutablePath=(Join-Path $fixture 'must-not-run-go.exe')
    ModuleCacheDirectory=(Join-Path $fixture 'unused-module-cache')
    BuildCacheDirectory=(Join-Path $fixture 'unused-build-cache')
    SpdxSchemaPath=(Join-Path $fixture 'unused-schema.json')
    JavaExecutablePath=(Join-Path $fixture 'must-not-run-java.exe')
    SpdxToolsJarPath=(Join-Path $fixture 'unused-validator.jar')
}
$producer=Join-Path $repositoryRoot 'scripts/Invoke-Sbom.ps1'
$message=$null
$accepted=@()
try {$accepted=@(& $producer @arguments)} catch {$message=$_.Exception.Message}
if ($message -cne 'Native SignTool refused the package signature.' -or $accepted.Count -ne 0 -or
    (Test-Path -LiteralPath $arguments.OutputDirectory) -or
    (Test-Path -LiteralPath (Join-Path $arguments.WorkingDirectory 'inspection')) -or
    (Test-Path -LiteralPath (Join-Path $arguments.WorkingDirectory 'generation'))) {
    throw "Release entry point failed to refuse the unsigned bundle before extraction: $message"
}
if (-not [IO.File]::Exists((Join-Path $arguments.WorkingDirectory 'signature.log'))) {
    throw 'Native signature refusal diagnostics were not retained.'
}
Write-Output 'PASS: unsigned release bundle rejected by native trust before extraction, cataloging or publication.'
foreach ($case in @('wrong-digest','existing-output','nested-output','same-destination')) {
    $candidate=$arguments.Clone()
    $candidate.OutputDirectory=Join-Path $fixture "$case-output"
    $candidate.WorkingDirectory=Join-Path $fixture "$case-work"
    $expected=switch ($case) {
        'wrong-digest' {$candidate.ExpectedBundleSha256='0'*64; 'bundle digest differs'}
        'existing-output' {
            [void][IO.Directory]::CreateDirectory($candidate.OutputDirectory)
            [IO.File]::WriteAllText((Join-Path $candidate.OutputDirectory 'sentinel.txt'),'preserve')
            'must be fresh and absent'
        }
        'nested-output' {$candidate.OutputDirectory=Join-Path $candidate.ComponentEvidenceRoot 'must-not-publish'; 'must be disjoint from inventory inputs'}
        'same-destination' {$candidate.OutputDirectory=$candidate.WorkingDirectory; 'must be disjoint and on the same volume'}
    }
    $message=$null
    try {& $producer @candidate | Out-Null} catch {$message=$_.Exception.Message}
    if ($null -eq $message -or -not $message.Contains($expected,[StringComparison]::Ordinal) -or
        (Test-Path -LiteralPath $candidate.WorkingDirectory)) {
        throw "Release preflight '$case' did not refuse before creating work: $message"
    }
    if ($case -ne 'existing-output' -and (Test-Path -LiteralPath $candidate.OutputDirectory)) {
        throw "Release preflight '$case' created an output."
    }
    if ($case -eq 'existing-output' -and [IO.File]::ReadAllText((Join-Path $candidate.OutputDirectory 'sentinel.txt')) -cne 'preserve') {
        throw 'Release refusal modified an existing output.'
    }
}
if ((Get-FileHash -LiteralPath $UnsignedBundlePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $hash) {
    throw 'Release qualification modified the original bundle.'
}
Write-Output 'PASS: release preflight rejects a wrong digest, existing output, nested input/output and identical destinations without side effects.'
