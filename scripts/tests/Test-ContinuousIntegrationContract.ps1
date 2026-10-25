<# Source-wiring contract for the actual hosted build matrix. Native workflow
   validation remains separate from hosted execution, which this test cannot prove. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ToolsDirectory,
    [string]$WorkflowPath = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) '.github/workflows/continuous-integration.yml')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $root 'scripts/QualificationPolicy.psm1')
$hostedPrerequisiteMinutes = [string](Get-QualificationTimeout -Category hostedPrerequisiteStep -Unit Minutes)
& (Join-Path $root 'scripts/Sync-QualificationPolicy.ps1') -ToolsDirectory $ToolsDirectory
$workflow = $WorkflowPath
if (-not (Test-Path -LiteralPath $workflow -PathType Leaf)) { throw 'The continuous integration workflow is absent.' }
& (Join-Path $root 'scripts/Test-GitHubWorkflowPolicy.ps1') -ToolsDirectory $ToolsDirectory -WorkflowPaths @($workflow)
$yaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
$reader = [IO.File]::OpenText($workflow)
try { $yaml.Load($reader) } finally { $reader.Dispose() }
function Get-NodeProperty { param($Mapping, [string]$Name); return , $Mapping.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new($Name)] }
$dispatch = Get-NodeProperty (Get-NodeProperty $yaml.Documents[0].RootNode 'on') 'workflow_dispatch'
if ($dispatch -isnot [YamlDotNet.RepresentationModel.YamlMappingNode]) {
    throw 'Dependency caching must require an explicit manual boolean opt-in, defaulting to false.'
}
$cacheInput = Get-NodeProperty (Get-NodeProperty $dispatch 'inputs') 'cache_dependencies'
if ((Get-NodeProperty $cacheInput 'type').Value -cne 'boolean' -or
    (Get-NodeProperty $cacheInput 'default').Value -cne 'false') {
    throw 'Dependency caching must require an explicit manual boolean opt-in, defaulting to false.'
}
$jobs = Get-NodeProperty $yaml.Documents[0].RootNode 'jobs'
foreach ($job in $jobs.Children.Values) {
    foreach ($step in (Get-NodeProperty $job 'steps').Children) {
        $uses = Get-NodeProperty $step 'uses'
        if ($null -ne $uses -and $uses.Value.StartsWith('actions/cache@', [StringComparison]::Ordinal) -and
            (Get-NodeProperty $step 'if').Value -cne 'inputs.cache_dependencies') {
            throw 'Cloud dependency cache restore/save must be gated by the explicit manual opt-in.'
        }
    }
}
foreach ($name in @('build', 'policy', 'asan', 'analysis', 'unsigned-package', 'interactive-inputs', 'arm64-runtime')) {
    if (-not $jobs.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new($name))) { throw "Missing required hosted job: $name" }
}
# A hosted image label does not promise the SDK selected by the project. The
# first post-checkout step must install/check it before native tool evaluation.
foreach ($name in @('build', 'asan', 'analysis', 'unsigned-package')) {
    $jobSteps = Get-NodeProperty (Get-NodeProperty $jobs $name) 'steps'
    $setupStep = $jobSteps.Children[1]
    $setupRun = Get-NodeProperty $setupStep 'run'
    $setupTimeout = Get-NodeProperty $setupStep 'timeout-minutes'
    if ($null -eq $setupRun -or
        $setupRun.Value -cne './scripts/Install-HostedWindowsSdk.ps1 -OutputDirectory artifacts/build-logs/windows-sdk' -or
        $null -eq $setupTimeout -or $setupTimeout.Value -cne $hostedPrerequisiteMinutes -or
        $null -ne (Get-NodeProperty $setupStep 'if')) {
        throw "Hosted SDK prerequisite must run unconditionally immediately after checkout with its canonical $hostedPrerequisiteMinutes-minute bound: $name"
    }
}
$hasSourcePairControl = $false
$hasSourceTrustControl = $false
$hasComdatIdentityControl = $false
$policyCommands = [Collections.Generic.List[string]]::new()
foreach ($step in (Get-NodeProperty (Get-NodeProperty $jobs 'policy') 'steps').Children) {
    $runKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey)) { $policyCommands.Add($step.Children[$runKey].Value) }
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/tests/Test-IndependentSourceBuildPair.ps1', [StringComparison]::Ordinal)) { $hasSourcePairControl = $true }
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/tests/Test-SourceCommitSignatureTrust.ps1', [StringComparison]::Ordinal)) { $hasSourceTrustControl = $true }
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/tests/Test-ReleaseComdatIdentityPolicy.ps1', [StringComparison]::Ordinal)) {
        throw 'Native COMDAT compilation must run in the SDK-prepared x64 Release build lane, not source policy.'
    }
}
foreach ($control in @('Test-ArtifactBinaryOutput.ps1', 'Test-ReleaseCandidateArchive.ps1', 'Test-ReleaseCandidateArchiveContext.ps1')) {
    if (-not ($policyCommands -join "`n").Contains("./scripts/tests/$control -OwnedOutputRoot `$env:RUNNER_TEMP", [StringComparison]::Ordinal)) {
        throw "Manual CI lacks the candidate archival control: $control"
    }
}
if (-not $hasSourcePairControl) { throw 'Manual CI lacks the real independent-checkout lifecycle control.' }
if (-not $hasSourceTrustControl) { throw 'Manual CI lacks the portable native source-signature trust control.' }
$build = Get-NodeProperty $jobs 'build'
if ($build.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new('permissions'))) { throw 'Compiler jobs must inherit only the read-only workflow token.' }
$strategy = Get-NodeProperty $build 'strategy'
$matrix = Get-NodeProperty $strategy 'matrix'
$include = Get-NodeProperty $matrix 'include'
$actual = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($entry in $include.Children) {
    $architecture = (Get-NodeProperty $entry 'architecture').Value
    $configuration = (Get-NodeProperty $entry 'configuration').Value
    if (-not $actual.Add("$architecture/$configuration")) { throw 'Duplicate hosted matrix entry.' }
}
if (-not $actual.SetEquals([string[]]@('x86/Debug', 'x86/Release', 'x64/Debug', 'x64/Release', 'ARM64/Release'))) {
    throw 'Hosted build matrix is not the exact approved five configurations.'
}
$steps = Get-NodeProperty $build 'steps'
$hasBuild = $false; $hasHeadlessTests = $false; $hasProvenance = $false; $hasShutdownRegression = $false
$hasAdobeStaticDebugInformation = $false
$hasExiv2CharsetQualification = $false
$hasNativeInlineTaskEnvironment = $false
$hasHostedRuntime = $false
foreach ($step in $steps.Children) {
    $key = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if (-not $step.Children.ContainsKey($key)) { continue }
    $command = $step.Children[$key].Value
    if ($command.Contains('scripts/Install-HostedWindowsAppRuntime.ps1', [StringComparison]::Ordinal)) {
        # Compilation needs the SDK; starting the unpackaged WinUI test host
        # additionally needs Microsoft's deployed runtime for the current user.
        $expectedRuntimeCommand = './scripts/Install-HostedWindowsAppRuntime.ps1 -Architecture ''${{ matrix.architecture }}'' -OutputDirectory artifacts/build-logs/windows-app-runtime'
        $runtimeGuard = Get-NodeProperty $step 'if'
        $runtimeTimeout = Get-NodeProperty $step 'timeout-minutes'
        if ($hasHostedRuntime -or -not $hasBuild -or $command -cne $expectedRuntimeCommand -or
            $null -eq $runtimeGuard -or $runtimeGuard.Value -cne "matrix.architecture != 'ARM64'" -or
            $null -eq $runtimeTimeout -or $runtimeTimeout.Value -cne $hostedPrerequisiteMinutes) {
            throw 'Hosted runtime deployment requires one bounded native x86/x64 setup after compilation and before test-host execution.'
        }
        $hasHostedRuntime = $true
    }
    if ($command.Contains('scripts/tests/Test-ReleaseComdatIdentityPolicy.ps1', [StringComparison]::Ordinal)) {
        $guard = Get-NodeProperty $step 'if'
        $expectedComdatCommand = './scripts/tests/Test-ReleaseComdatIdentityPolicy.ps1 -OwnedOutputRoot ' +
        '(Join-Path $env:GITHUB_WORKSPACE ''artifacts/build-logs/x64/Release/ComdatIdentity'')'
        if ($null -eq $guard -or $guard.Value -cne "matrix.architecture == 'x64' && matrix.configuration == 'Release'" -or
            $command -cne $expectedComdatCommand) {
            throw 'Native COMDAT control requires the SDK-prepared x64 Release lane and retained build-log output.'
        }
        $hasComdatIdentityControl = $true
    }
    if ($command.Contains('scripts/tests/Test-NativeInlineTaskEnvironment.ps1', [StringComparison]::Ordinal)) {
        # Every lane must exercise its actual selected native environment, and
        # its raw failure evidence must live under the always-uploaded log root.
        if ($step.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new('if')) -or
            -not $command.Contains('matrix.architecture', [StringComparison]::Ordinal) -or
            -not $command.Contains('matrix.configuration', [StringComparison]::Ordinal) -or
            -not $command.Contains('artifacts/build-logs/', [StringComparison]::Ordinal) -or
            -not $command.Contains('-Project JpgSpinner.JpegTransformation.Tests -EnableASAN', [StringComparison]::Ordinal) -or
            -not $command.Contains('-Project JpgSpinner.FuzzToolchain.Smoke -Configuration Fuzz', [StringComparison]::Ordinal) -or
            -not $command.Contains('-Project JpgSpinner.JpegSegmentScanner.Fuzz -Configuration Fuzz', [StringComparison]::Ordinal)) {
            throw 'Native inline-task regression requires every actual matrix lane and retained build logs.'
        }
        $hasNativeInlineTaskEnvironment = $true
    }
    if ($command.Contains('scripts/tests/Test-Exiv2WindowsCharset.ps1', [StringComparison]::Ordinal)) {
        if ($null -ne (Get-NodeProperty $step 'if') -or
            -not $command.Contains('-InstalledDependencyPrefix $prefix', [StringComparison]::Ordinal) -or
            -not $command.Contains('-WorkingDirectory (Join-Path $env:RUNNER_TEMP ''exiv2-charset'')', [StringComparison]::Ordinal) -or
            -not $command.Contains('-EvidenceDirectory (Join-Path $PWD ''artifacts/build-logs/Exiv2Charset'')', [StringComparison]::Ordinal)) {
            throw 'Installed Exiv2 charset qualification must run in every actual matrix lane with separate retained diagnostics.'
        }
        $hasExiv2CharsetQualification = $true
    }
    if ($command.Contains('scripts/tests/Test-AdobeXmpStaticDebugInformation.ps1', [StringComparison]::Ordinal)) {
        # Exercise the installed package in every native matrix configuration,
        # not merely one standalone developer probe or Release-only shortcut.
        if ($step.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new('if')) -or
            -not $command.Contains('-InstalledDependencyPrefix', [StringComparison]::Ordinal) -or
            -not $command.Contains("matrix.architecture", [StringComparison]::Ordinal) -or
            -not $command.Contains("matrix.configuration", [StringComparison]::Ordinal)) {
            throw 'Installed Adobe debug-information regression must use every actual matrix configuration.'
        }
        $hasAdobeStaticDebugInformation = $true
    }
    if ($command.Contains('scripts/tests/Test-PresentationHostShutdown.ps1 -Scenario RecoveryDiagnostic', [StringComparison]::Ordinal)) {
        if (-not $hasHostedRuntime) { throw 'Hosted runtime deployment must precede unpackaged presentation test-host execution.' }
        $condition = (Get-NodeProperty $step 'if').Value
        if ($condition -cne "matrix.architecture == 'x64' && matrix.configuration == 'Release'") {
            throw 'The bounded shutdown regression requires its actual x64 Release host.'
        }
        $hasShutdownRegression = $true
    }
    if ($command.Contains('scripts/Invoke-Build.ps1', [StringComparison]::Ordinal)) { $hasBuild = $true }
    if ($command.Contains('scripts/New-BuildProvenance.ps1', [StringComparison]::Ordinal) -and
        $command.Contains('-RequireCleanSourceTree', [StringComparison]::Ordinal)) { $hasProvenance = $true }
    if ($command.Contains('scripts/Invoke-TestSuite.ps1 -Project HeadlessAll', [StringComparison]::Ordinal)) {
        if (-not $hasHostedRuntime) { throw 'Hosted runtime deployment must precede unpackaged presentation test-host execution.' }
        $condition = (Get-NodeProperty $step 'if').Value
        if ($condition -cne "matrix.architecture != 'ARM64'") { throw 'Cross-compiled ARM64 must not masquerade as executed tests.' }
        $hasHeadlessTests = $true
    }
}
if (-not ($hasBuild -and $hasHeadlessTests -and $hasProvenance)) { throw 'Build, native headless tests or clean-source provenance wiring is absent.' }
if (-not $hasShutdownRegression) { throw 'Bounded native presentation process-shutdown regression wiring is absent.' }
if (-not $hasAdobeStaticDebugInformation) { throw 'Installed Adobe static-library debug-information regression wiring is absent.' }
if (-not $hasExiv2CharsetQualification) { throw 'Installed Exiv2 Windows charset qualification wiring is absent.' }
if (-not $hasNativeInlineTaskEnvironment) { throw 'Native inline-task environment regression wiring is absent.' }
if (-not $hasComdatIdentityControl) { throw 'Manual CI lacks the native x64 Release COMDAT identity control.' }

# Native execution is a separate deployment-only job, consuming only this run's
# immutable ARM64 test-input artifact. A cross-build or x64 emulation cannot close it.
$arm64 = Get-NodeProperty $jobs 'arm64-runtime'
if ((Get-NodeProperty $arm64 'runs-on').Value -cne 'windows-11-vs2026-arm' -or
    (Get-NodeProperty $arm64 'needs').Value -cne 'build' -or
    $null -ne (Get-NodeProperty $arm64 'permissions')) {
    throw 'ARM64 runtime requires the native hosted VM, successful same-run build and inherited read-only permissions.'
}
$hasArm64Download = $false; $hasArm64Runtime = $false; $hasArm64Tests = $false; $hasArm64Reports = $false; $hasArm64Upload = $false
foreach ($step in (Get-NodeProperty $arm64 'steps').Children) {
    $run = Get-NodeProperty $step 'run'
    if ($null -ne $run) {
        if ($run.Value.Contains('Invoke-Build.ps1', [StringComparison]::Ordinal) -or
            $run.Value.Contains('Install-HostedWindowsSdk.ps1', [StringComparison]::Ordinal)) {
            throw 'ARM64 runtime job must deploy prebuilt inputs without compilation or SDK installation.'
        }
        if ($run.Value.Contains('Install-HostedWindowsAppRuntime.ps1', [StringComparison]::Ordinal)) {
            if (-not $hasArm64Download -or $null -ne (Get-NodeProperty $step 'if') -or
                (Get-NodeProperty $step 'timeout-minutes').Value -cne $hostedPrerequisiteMinutes -or
                $run.Value -cne './scripts/Install-HostedWindowsAppRuntime.ps1 -Architecture ARM64 -OutputDirectory artifacts/build-logs/windows-app-runtime') {
                throw 'ARM64 runtime setup must deploy the bounded native prerequisite after same-run download.'
            }
            $hasArm64Runtime = $true
        }
        if ($run.Value.Contains('Invoke-TestSuite.ps1', [StringComparison]::Ordinal)) {
            if (-not $hasArm64Runtime -or $null -ne (Get-NodeProperty $step 'if') -or
                $run.Value.Trim() -cne ('./scripts/Invoke-TestSuite.ps1 -Project HeadlessAll -Configuration Release -Architecture ARM64 -RequireNativeArchitecture' +
                    "`nif (`$LASTEXITCODE -ne 0) { exit `$LASTEXITCODE }")) {
                throw 'ARM64 headless tests must require actual native machine identities and propagate their exit code after runtime deployment.'
            }
            $hasArm64Tests = $true
        }
    }
    $uses = Get-NodeProperty $step 'uses'
    if ($null -eq $uses) { continue }
    $arguments = Get-NodeProperty $step 'with'
    if ($uses.Value.StartsWith('actions/download-artifact@', [StringComparison]::Ordinal)) {
        if ($arguments.Children.Count -ne 3 -or (Get-NodeProperty $arguments 'name').Value -cne 'arm64-headless-test-inputs' -or
            (Get-NodeProperty $arguments 'path').Value -cne 'artifacts/bin/ARM64/Release' -or
            (Get-NodeProperty $arguments 'digest-mismatch').Value -cne 'error') {
            throw 'ARM64 execution requires exact same-run artifact selection and native digest refusal.'
        }
        $hasArm64Download = $true
    }
    if ($uses.Value.StartsWith('actions/upload-artifact@', [StringComparison]::Ordinal)) {
        if ((Get-NodeProperty $step 'if').Value -cne 'always()' -or
            (Get-NodeProperty $arguments 'name').Value -cne 'arm64-runtime-evidence' -or
            (Get-NodeProperty $arguments 'path').Value.Trim() -cne "artifacts/build-logs/`nartifacts/TestResults/`nartifacts/provenance/") {
            throw 'ARM64 runtime must retain original diagnostics, architecture identities and test reports even on failure.'
        }
        $hasArm64Reports = $true
    }
}
foreach ($step in $steps.Children) {
    $uses = Get-NodeProperty $step 'uses'
    if ($null -eq $uses -or -not $uses.Value.StartsWith('actions/upload-artifact@', [StringComparison]::Ordinal)) { continue }
    $arguments = Get-NodeProperty $step 'with'
    if ((Get-NodeProperty $arguments 'name').Value -cne 'arm64-headless-test-inputs') { continue }
    if ((Get-NodeProperty $step 'if').Value -cne "matrix.architecture == 'ARM64' && matrix.configuration == 'Release'" -or
        (Get-NodeProperty $arguments 'path').Value.Trim() -cne "artifacts/bin/ARM64/Release/*.Tests/**`nartifacts/bin/ARM64/Release/JpgSpinner.App/AppxManifest.xml`n!artifacts/bin/ARM64/Release/**/*.pdb") {
        throw 'ARM64 test transfer must include the native manifest fixture and Release test hosts while excluding reconstructible PDBs.'
    }
    $hasArm64Upload = $true
}
if (-not ($hasArm64Upload -and $hasArm64Download -and $hasArm64Runtime -and $hasArm64Tests -and $hasArm64Reports)) {
    throw 'ARM64 native execution transfer, runtime, tests or retained reports are absent.'
}
$packageJob = Get-NodeProperty $jobs 'unsigned-package'
$packageNeeds = Get-NodeProperty $packageJob 'needs'
$neededJobs = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($dependency in $packageNeeds.Children) { [void]$neededJobs.Add($dependency.Value) }
if (-not $neededJobs.SetEquals([string[]]@('policy', 'build', 'arm64-runtime', 'asan', 'analysis'))) {
    throw 'Unsigned packaging must wait for every automated qualification job.'
}
$packageSteps = Get-NodeProperty $packageJob 'steps'
$hasNativePackageBuild = $false; $hasNativeBundle = $false; $hasBundleInspection = $false
$hasPayloadComparisonControls = $false; $hasBlockIntegrityControl = $false; $hasWinrtOriginControl = $false; $hasStoreComponentControl = $false
foreach ($step in $packageSteps.Children) {
    $key = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if (-not $step.Children.ContainsKey($key)) { continue }
    $command = $step.Children[$key].Value
    if ($command.Contains('scripts/tests/Test-PackageBuildContract.ps1', [StringComparison]::Ordinal) -and
        $command.Contains("@('x86','x64','ARM64')", [StringComparison]::Ordinal)) { $hasNativePackageBuild = $true }
    if ($command.Contains('scripts/New-MsixBundle.ps1', [StringComparison]::Ordinal)) { $hasNativeBundle = $true }
    if ($command.Contains('scripts/Expand-JpgSpinnerBundle.ps1', [StringComparison]::Ordinal)) { $hasBundleInspection = $true }
    if ($command.Contains('scripts/tests/Test-JpgSpinnerBundlePayloadComparison.ps1 -BundlePath $bundle', [StringComparison]::Ordinal)) { $hasPayloadComparisonControls = $true }
    if ($command.Contains('scripts/tests/Test-NativePackageBlockIntegrity.ps1 -PackagePath $packages[1]', [StringComparison]::Ordinal)) { $hasBlockIntegrityControl = $true }
    if ($command.Contains('scripts/tests/Test-WinrtErrorSourceLocationPolicy.ps1 -WinrtProjectionDirectory', [StringComparison]::Ordinal)) { $hasWinrtOriginControl = $true }
    if ($command.Contains('scripts/tests/Test-StoreSubmissionPackageBuild.ps1', [StringComparison]::Ordinal)) { $hasStoreComponentControl = $true }
}
if (-not ($hasPayloadComparisonControls -and $hasBlockIntegrityControl)) { throw 'Unsigned packaging lacks native closed-comparison and false-block-hash controls.' }
if (-not $hasWinrtOriginControl) { throw 'Unsigned packaging lacks the native Release/Debug error-origin policy control.' }
if (-not $hasStoreComponentControl) { throw 'Unsigned packaging lacks the native Store submission-component build control.' }
if (-not ($hasNativePackageBuild -and $hasNativeBundle -and $hasBundleInspection)) {
    throw 'Unsigned packaging lacks the real architecture producer, native bundle or payload inspection.'
}
$interactivePublisher = Get-NodeProperty $jobs 'interactive-inputs'
$publisherNeeds = Get-NodeProperty $interactivePublisher 'needs'
$neededJobs.Clear()
foreach ($dependency in $publisherNeeds.Children) { [void]$neededJobs.Add($dependency.Value) }
if (-not $neededJobs.SetEquals([string[]]@('policy', 'build', 'arm64-runtime', 'asan', 'analysis'))) { throw 'Interactive input publication must wait for all automated qualification jobs.' }
if ((Get-NodeProperty $interactivePublisher 'if').Value -cne "github.ref == 'refs/heads/main'") { throw 'Interactive attestation must be restricted to the trusted main ref.' }
$hasDropProducer = $false
foreach ($step in $steps.Children) {
    $runKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/New-InteractiveTestDrop.ps1', [StringComparison]::Ordinal)) {
        if ((Get-NodeProperty $step 'if').Value -cne "matrix.architecture == 'x64' && matrix.configuration == 'Release'") { throw 'Interactive inputs must use the real x64 Release producer only.' }
        $hasDropProducer = $true
    }
}
if (-not $hasDropProducer) { throw 'The prebuilt interactive input producer is absent from the hosted build.' }
$publisherSteps = Get-NodeProperty $interactivePublisher 'steps'
$publisherPermissions = Get-NodeProperty $interactivePublisher 'permissions'
$actualPermissions = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($permission in $publisherPermissions.Children.GetEnumerator()) { [void]$actualPermissions.Add("$($permission.Key.Value):$($permission.Value.Value)") }
if (-not $actualPermissions.SetEquals([string[]]@('contents:read', 'actions:read', 'id-token:write', 'attestations:write'))) { throw 'Input publisher requires exactly the native platform signing permissions.' }
$hasInputValidation = $false; $hasInputAttestation = $false; $hasDownload = $false; $hasPublication = $false
foreach ($step in $publisherSteps.Children) {
    $runKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey) -and $step.Children[$runKey].Value.Contains('scripts/Test-InteractiveTestDrop.ps1', [StringComparison]::Ordinal)) { $hasInputValidation = $true }
    $usesKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('uses')
    if (-not $step.Children.ContainsKey($usesKey)) { continue }
    $uses = $step.Children[$usesKey].Value
    $arguments = Get-NodeProperty $step 'with'
    if ($uses.StartsWith('actions/download-artifact@', [StringComparison]::Ordinal)) {
        if ((Get-NodeProperty $arguments 'name').Value -cne 'unsigned-interactive-inputs' -or
            (Get-NodeProperty $arguments 'digest-mismatch').Value -cne 'error') { throw 'Publisher must download exact inputs with native digest failure enabled.' }
        $hasDownload = $true
    }
    if ($uses.StartsWith('actions/attest@', [StringComparison]::Ordinal)) {
        if (-not ($hasDownload -and $hasInputValidation)) { throw 'Input attestation precedes download or independent byte validation.' }
        if ((Get-NodeProperty $arguments 'subject-path').Value -cne 'artifacts/interactive-inputs/build-context.json' -or
            (Get-NodeProperty $arguments 'predicate-path').Value -cne 'artifacts/interactive-inputs/build-context.json' -or
            (Get-NodeProperty $arguments 'predicate-type').Value -cne 'https://haddenindustries.com/schemas/jpg-spinner/interactive-test-inputs/v1') { throw 'Publisher attests the wrong subject or input predicate.' }
        $hasInputAttestation = $true
    }
    if ($uses.StartsWith('actions/upload-artifact@', [StringComparison]::Ordinal)) {
        if (-not $hasInputAttestation -or (Get-NodeProperty $arguments 'name').Value -cne 'interactive-test-drop' -or
            (Get-NodeProperty $arguments 'if-no-files-found').Value -cne 'error') { throw 'Complete input publication must follow attestation.' }
        $hasPublication = $true
    }
}
if (-not ($hasDownload -and $hasInputValidation -and $hasInputAttestation -and $hasPublication)) { throw 'Prebuilt interactive inputs require native validation and platform attestation before publication.' }
# The regression driver now owns its exact output reservation. Keep that
# reservation under the same upload root as the later fuzz run, so a failed
# regression cannot lose its native streams/corpus when RUNNER_TEMP disappears.
# This checks the repository's chosen command binding, not PowerShell grammar.
$scannerWorkflow = Join-Path $root '.github/workflows/scanner-fuzz.yml'
$scannerYaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
$reader = [IO.File]::OpenText($scannerWorkflow)
try { $scannerYaml.Load($reader) } finally { $reader.Dispose() }
$scannerJob = Get-NodeProperty (Get-NodeProperty $scannerYaml.Documents[0].RootNode 'jobs') 'scanner'
$scannerSdkStep = (Get-NodeProperty $scannerJob 'steps').Children[1]
if ((Get-NodeProperty $scannerSdkStep 'run').Value -cne './scripts/Install-HostedWindowsSdk.ps1 -OutputDirectory artifacts/fuzz/scanner/windows-sdk' -or
    (Get-NodeProperty $scannerSdkStep 'timeout-minutes').Value -cne $hostedPrerequisiteMinutes -or $null -ne (Get-NodeProperty $scannerSdkStep 'if')) {
    throw 'Scanner compiler prerequisites require unconditional bounded SDK setup immediately after checkout.'
}
$hasPreservedRegression = $false
$hasScannerUpload = $false
foreach ($step in (Get-NodeProperty $scannerJob 'steps').Children) {
    $runKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey) -and
        $step.Children[$runKey].Value.Contains('scripts/tests/Test-ScannerCorpusMinimization.ps1',
            [StringComparison]::Ordinal)) {
        $expectedCommand = './scripts/tests/Test-ScannerCorpusMinimization.ps1 -OwnedOutputRoot ' +
        '(Join-Path $env:GITHUB_WORKSPACE ''artifacts/fuzz/scanner/regression'')'
        if ($step.Children[$runKey].Value -cne $expectedCommand) {
            throw 'Scanner regression must reserve its output beneath the uploaded scanner artifact root.'
        }
        $hasPreservedRegression = $true
    }
    $usesKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('uses')
    if ($step.Children.ContainsKey($usesKey) -and
        $step.Children[$usesKey].Value.StartsWith('actions/upload-artifact@', [StringComparison]::Ordinal)) {
        $arguments = Get-NodeProperty $step 'with'
        if ((Get-NodeProperty $arguments 'path').Value -cne 'artifacts/fuzz/scanner/' -or
            (Get-NodeProperty $step 'if').Value -cne 'always()') {
            throw 'Scanner artifact upload must preserve the complete regression/fuzz root even after failure.'
        }
        $hasScannerUpload = $true
    }
}
if (-not ($hasPreservedRegression -and $hasScannerUpload)) {
    throw 'Scanner regression and its unconditional artifact upload must both be present.'
}
Write-Output 'PASS: native-validated CI source wiring and exact build/test architecture matrix; hosted execution remains unqualified.'
