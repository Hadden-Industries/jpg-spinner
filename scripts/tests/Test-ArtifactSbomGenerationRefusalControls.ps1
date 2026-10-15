<# Exercise preflight refusals with real filesystem objects and unreachable
   tool paths. No native generation or build is needed for these contracts. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$OwnedOutputRoot,
    [Parameter(Mandatory)][string]$SpdxSchemaPath,
    [Parameter(Mandatory)][string]$SpdxToolsJarPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $OwnedOutputRoot ('generation-refusal-' + [Guid]::NewGuid().ToString('N'))
$payload = Join-Path $fixture 'payload'
$components = Join-Path $fixture 'components'
$ordinary = Join-Path $fixture 'ordinary'
foreach ($directory in @($payload,$components,$ordinary)) { [void][IO.Directory]::CreateDirectory($directory) }
$junction = Join-Path $fixture 'redirected'
[void](New-Item -ItemType Junction -Path $junction -Target $ordinary)
if (([IO.File]::GetAttributes($junction) -band [IO.FileAttributes]::ReparsePoint) -eq 0) { throw 'Real junction fixture is absent.' }
$sentinel = Join-Path $fixture 'existing.spdx.json'
[IO.File]::WriteAllText($sentinel,'preserve existing output')
$sentinelHash = (Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash
$arguments = @{
    PayloadRoot=$payload;ComponentEvidenceRoot=$components
    ComponentContractPath=(Join-Path $repositoryRoot 'tests/TestData/SbomContractFixture/expected-contract.json')
    PublicArtifactName='JpgSpinner.OverlayQualification';PublicArtifactVersion='2.0.0'
    GoExecutablePath='must-not-execute-go.exe';JavaExecutablePath='must-not-execute-java.exe'
    ModuleCacheDirectory=$fixture;BuildCacheDirectory=$fixture
    SpdxSchemaPath=$SpdxSchemaPath;SpdxToolsJarPath=$SpdxToolsJarPath
}
foreach ($case in @(
    @{Name='existing-output';Output=$sentinel;Working=(Join-Path $fixture 'existing-work');Diagnostic='fresh and absent'},
    @{Name='input-output';Output=(Join-Path $payload 'must-not-publish');Working=(Join-Path $fixture 'input-work');Diagnostic='outside inventory inputs'},
    @{Name='wrong-pin';Output=(Join-Path $fixture 'wrong-pin.spdx.json');Working=(Join-Path $fixture 'wrong-pin-work');Diagnostic='validator identity differs';Schema=$sentinel},
    @{Name='junction';Output=(Join-Path $fixture 'junction.spdx.json');Working=(Join-Path $junction 'must-not-create');Diagnostic='reparse-point'}
)) {
    $caseArguments = $arguments.Clone()
    if ($case.ContainsKey('Schema')) { $caseArguments.SpdxSchemaPath=$case.Schema }
    $message = $null
    try {
        & (Join-Path $repositoryRoot 'scripts/New-ArtifactSbom.ps1') @caseArguments -OutputPath $case.Output -WorkingDirectory $case.Working | Out-Null
    } catch { $message=$_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic,[StringComparison]::Ordinal) -or
        (Test-Path -LiteralPath $case.Working) -or ($case.Output -cne $sentinel -and (Test-Path -LiteralPath $case.Output))) {
        throw "Generation refusal '$($case.Name)' failed or had effects: $message"
    }
}
if ((Get-FileHash -LiteralPath $sentinel -Algorithm SHA256).Hash -cne $sentinelHash -or
    @(Get-ChildItem -LiteralPath $payload -Force).Count -ne 0 -or @(Get-ChildItem -LiteralPath $components -Force).Count -ne 0) {
    throw 'Generation preflight altered existing output or inventory inputs.'
}
Write-Output 'PASS: existing output, input containment, wrong validator pin and real ancestor junction refuse before tools or output creation.'
