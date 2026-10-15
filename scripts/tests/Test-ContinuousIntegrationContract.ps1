<# Source-wiring contract for the actual hosted build matrix. Native workflow
   validation remains separate from hosted execution, which this test cannot prove. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$workflow = Join-Path $root '.github/workflows/continuous-integration.yml'
if (-not (Test-Path -LiteralPath $workflow -PathType Leaf)) { throw 'The continuous integration workflow is absent.' }
& (Join-Path $root 'scripts/Test-GitHubWorkflowPolicy.ps1') -ToolsDirectory $ToolsDirectory -WorkflowPaths @($workflow)
$yaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
$reader = [IO.File]::OpenText($workflow)
try { $yaml.Load($reader) } finally { $reader.Dispose() }
function Get-NodeProperty { param($Mapping,[string]$Name); return ,$Mapping.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new($Name)] }
$jobs = Get-NodeProperty $yaml.Documents[0].RootNode 'jobs'
foreach ($name in @('build','policy','asan','analysis','unsigned-package','interactive-inputs')) {
    if (-not $jobs.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new($name))) { throw "Missing required hosted job: $name" }
}
$hasSourcePairControl=$false
$hasSourceTrustControl=$false
$hasComdatIdentityControl=$false
foreach ($step in (Get-NodeProperty (Get-NodeProperty $jobs 'policy') 'steps').Children) {
    $runKey=[YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/tests/Test-IndependentSourceBuildPair.ps1',[StringComparison]::Ordinal)) {$hasSourcePairControl=$true}
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/tests/Test-SourceCommitSignatureTrust.ps1',[StringComparison]::Ordinal)) {$hasSourceTrustControl=$true}
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/tests/Test-ReleaseComdatIdentityPolicy.ps1',[StringComparison]::Ordinal)) {$hasComdatIdentityControl=$true}
}
if (-not $hasSourcePairControl) {throw 'Manual CI lacks the real independent-checkout lifecycle control.'}
if (-not $hasSourceTrustControl) {throw 'Manual CI lacks the portable native source-signature trust control.'}
if (-not $hasComdatIdentityControl) {throw 'Manual CI lacks the native x64 Release COMDAT identity control.'}
$build = Get-NodeProperty $jobs 'build'
if ($build.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new('permissions'))) {throw 'Compiler jobs must inherit only the read-only workflow token.'}
$strategy = Get-NodeProperty $build 'strategy'
$matrix = Get-NodeProperty $strategy 'matrix'
$include = Get-NodeProperty $matrix 'include'
$actual = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($entry in $include.Children) {
    $architecture = (Get-NodeProperty $entry 'architecture').Value
    $configuration = (Get-NodeProperty $entry 'configuration').Value
    if (-not $actual.Add("$architecture/$configuration")) { throw 'Duplicate hosted matrix entry.' }
}
if (-not $actual.SetEquals([string[]]@('x86/Debug','x86/Release','x64/Debug','x64/Release','ARM64/Release'))) {
    throw 'Hosted build matrix is not the exact approved five configurations.'
}
$steps = Get-NodeProperty $build 'steps'
$hasBuild=$false;$hasHeadlessTests=$false;$hasProvenance=$false;$hasShutdownRegression=$false
foreach ($step in $steps.Children) {
    $key = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if (-not $step.Children.ContainsKey($key)) { continue }
    $command = $step.Children[$key].Value
    if ($command.Contains('scripts/tests/Test-PresentationHostShutdown.ps1 -Scenario RecoveryDiagnostic',[StringComparison]::Ordinal)) {
        $condition = (Get-NodeProperty $step 'if').Value
        if ($condition -cne "matrix.architecture == 'x64' && matrix.configuration == 'Release'") {
            throw 'The bounded shutdown regression requires its actual x64 Release host.'
        }
        $hasShutdownRegression=$true
    }
    if ($command.Contains('scripts/Invoke-Build.ps1',[StringComparison]::Ordinal)) { $hasBuild=$true }
    if ($command.Contains('scripts/New-BuildProvenance.ps1',[StringComparison]::Ordinal) -and
        $command.Contains('-RequireCleanSourceTree',[StringComparison]::Ordinal)) { $hasProvenance=$true }
    if ($command.Contains('scripts/Invoke-TestSuite.ps1 -Project HeadlessAll',[StringComparison]::Ordinal)) {
        $condition = (Get-NodeProperty $step 'if').Value
        if ($condition -cne "matrix.architecture != 'ARM64'") { throw 'Cross-compiled ARM64 must not masquerade as executed tests.' }
        $hasHeadlessTests=$true
    }
}
if (-not ($hasBuild -and $hasHeadlessTests -and $hasProvenance)) { throw 'Build, native headless tests or clean-source provenance wiring is absent.' }
if (-not $hasShutdownRegression) { throw 'Bounded native presentation process-shutdown regression wiring is absent.' }
$packageJob = Get-NodeProperty $jobs 'unsigned-package'
$packageNeeds = Get-NodeProperty $packageJob 'needs'
$neededJobs = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($dependency in $packageNeeds.Children) { [void]$neededJobs.Add($dependency.Value) }
if (-not $neededJobs.SetEquals([string[]]@('policy','build','asan','analysis'))) {
    throw 'Unsigned packaging must wait for every automated qualification job.'
}
$packageSteps = Get-NodeProperty $packageJob 'steps'
$hasNativePackageBuild=$false; $hasNativeBundle=$false; $hasBundleInspection=$false
$hasPayloadComparisonControls=$false;$hasBlockIntegrityControl=$false;$hasWinrtOriginControl=$false;$hasStoreComponentControl=$false
foreach ($step in $packageSteps.Children) {
    $key = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if (-not $step.Children.ContainsKey($key)) { continue }
    $command = $step.Children[$key].Value
    if ($command.Contains('scripts/tests/Test-PackageBuildContract.ps1',[StringComparison]::Ordinal) -and
        $command.Contains("@('x86','x64','ARM64')",[StringComparison]::Ordinal)) { $hasNativePackageBuild=$true }
    if ($command.Contains('scripts/New-MsixBundle.ps1',[StringComparison]::Ordinal)) { $hasNativeBundle=$true }
    if ($command.Contains('scripts/Expand-JpgSpinnerBundle.ps1',[StringComparison]::Ordinal)) { $hasBundleInspection=$true }
    if ($command.Contains('scripts/tests/Test-JpgSpinnerBundlePayloadComparison.ps1 -BundlePath $bundle',[StringComparison]::Ordinal)) {$hasPayloadComparisonControls=$true}
    if ($command.Contains('scripts/tests/Test-NativePackageBlockIntegrity.ps1 -PackagePath $packages[1]',[StringComparison]::Ordinal)) {$hasBlockIntegrityControl=$true}
    if ($command.Contains('scripts/tests/Test-WinrtErrorSourceLocationPolicy.ps1 -WinrtProjectionDirectory',[StringComparison]::Ordinal)) {$hasWinrtOriginControl=$true}
    if ($command.Contains('scripts/tests/Test-StoreSubmissionPackageBuild.ps1',[StringComparison]::Ordinal)) {$hasStoreComponentControl=$true}
}
if (-not ($hasPayloadComparisonControls -and $hasBlockIntegrityControl)) {throw 'Unsigned packaging lacks native closed-comparison and false-block-hash controls.'}
if (-not $hasWinrtOriginControl) {throw 'Unsigned packaging lacks the native Release/Debug error-origin policy control.'}
if (-not $hasStoreComponentControl) {throw 'Unsigned packaging lacks the native Store submission-component build control.'}
if (-not ($hasNativePackageBuild -and $hasNativeBundle -and $hasBundleInspection)) {
    throw 'Unsigned packaging lacks the real architecture producer, native bundle or payload inspection.'
}
$interactivePublisher=Get-NodeProperty $jobs 'interactive-inputs'
$publisherNeeds=Get-NodeProperty $interactivePublisher 'needs'
$neededJobs.Clear()
foreach ($dependency in $publisherNeeds.Children) {[void]$neededJobs.Add($dependency.Value)}
if (-not $neededJobs.SetEquals([string[]]@('policy','build','asan','analysis'))) {throw 'Interactive input publication must wait for all automated qualification jobs.'}
if ((Get-NodeProperty $interactivePublisher 'if').Value -cne "github.ref == 'refs/heads/main'") {throw 'Interactive attestation must be restricted to the trusted main ref.'}
$hasDropProducer=$false
foreach ($step in $steps.Children) {
    $runKey=[YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/New-InteractiveTestDrop.ps1',[StringComparison]::Ordinal)) {
        if ((Get-NodeProperty $step 'if').Value -cne "matrix.architecture == 'x64' && matrix.configuration == 'Release'") {throw 'Interactive inputs must use the real x64 Release producer only.'}
        $hasDropProducer=$true
    }
}
if (-not $hasDropProducer) {throw 'The prebuilt interactive input producer is absent from the hosted build.'}
$publisherSteps=Get-NodeProperty $interactivePublisher 'steps'
$publisherPermissions=Get-NodeProperty $interactivePublisher 'permissions'
$actualPermissions=[Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($permission in $publisherPermissions.Children.GetEnumerator()) {[void]$actualPermissions.Add("$($permission.Key.Value):$($permission.Value.Value)")}
if (-not $actualPermissions.SetEquals([string[]]@('contents:read','actions:read','id-token:write','attestations:write'))) {throw 'Input publisher requires exactly the native platform signing permissions.'}
$hasInputValidation=$false;$hasInputAttestation=$false;$hasDownload=$false;$hasPublication=$false
foreach ($step in $publisherSteps.Children) {
    $runKey=[YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/Test-InteractiveTestDrop.ps1',[StringComparison]::Ordinal)) {$hasInputValidation=$true}
    $usesKey=[YamlDotNet.RepresentationModel.YamlScalarNode]::new('uses')
    if (-not $step.Children.ContainsKey($usesKey)) {continue}
    $uses=$step.Children[$usesKey].Value
    $arguments=Get-NodeProperty $step 'with'
    if ($uses.StartsWith('actions/download-artifact@',[StringComparison]::Ordinal)) {
        if ((Get-NodeProperty $arguments 'name').Value -cne 'unsigned-interactive-inputs' -or
            (Get-NodeProperty $arguments 'digest-mismatch').Value -cne 'error') {throw 'Publisher must download exact inputs with native digest failure enabled.'}
        $hasDownload=$true
    }
    if ($uses.StartsWith('actions/attest@',[StringComparison]::Ordinal)) {
        if (-not ($hasDownload -and $hasInputValidation)) {throw 'Input attestation precedes download or independent byte validation.'}
        if ((Get-NodeProperty $arguments 'subject-path').Value -cne 'artifacts/interactive-inputs/build-context.json' -or
            (Get-NodeProperty $arguments 'predicate-path').Value -cne 'artifacts/interactive-inputs/build-context.json' -or
            (Get-NodeProperty $arguments 'predicate-type').Value -cne 'https://haddenindustries.com/schemas/jpg-spinner/interactive-test-inputs/v1') {throw 'Publisher attests the wrong subject or input predicate.'}
        $hasInputAttestation=$true
    }
    if ($uses.StartsWith('actions/upload-artifact@',[StringComparison]::Ordinal)) {
        if (-not $hasInputAttestation -or (Get-NodeProperty $arguments 'name').Value -cne 'interactive-test-drop' -or
            (Get-NodeProperty $arguments 'if-no-files-found').Value -cne 'error') {throw 'Complete input publication must follow attestation.'}
        $hasPublication=$true
    }
}
if (-not ($hasDownload -and $hasInputValidation -and $hasInputAttestation -and $hasPublication)) {throw 'Prebuilt interactive inputs require native validation and platform attestation before publication.'}
Write-Output 'PASS: native-validated CI source wiring and exact build/test architecture matrix; hosted execution remains unqualified.'
