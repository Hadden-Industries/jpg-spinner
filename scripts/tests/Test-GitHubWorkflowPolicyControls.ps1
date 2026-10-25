<# Mutate one independently authored valid workflow per case. Each check
   invokes the actual policy and native validators, without running jobs. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory, [Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repositoryRoot 'scripts/QualificationPolicy.psm1')
$hostedPrerequisiteMinutes = Get-QualificationTimeout -Category hostedPrerequisiteStep -Unit Minutes
$policy = Join-Path $repositoryRoot 'scripts/Test-GitHubWorkflowPolicy.ps1'
$fixture = Join-Path $PSScriptRoot 'WorkflowFixtures/Valid.yml'
& $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($fixture)
$text = Get-Content -LiteralPath $fixture -Raw
$originalRun = "      - run: Write-Output 'Static fixture only; never executed by its validator.'"
$minimalUpload = @'
      - uses: actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9 # v7.0.2
        with:
          path: report.txt
          retention-days: 1
          compression-level: 9
'@
$fixtureRoot = Join-Path $OwnedOutputRoot ('workflow-policy-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
# Relative input paths are interpreted in the caller's directory, while native
# child execution deliberately occurs in the repository. Both readers must
# consume the same file rather than silently rebasing it after Push-Location.
Copy-Item -LiteralPath $fixture -Destination (Join-Path $fixtureRoot 'Valid.yml')
# Exercise the minimum native retention value positively before refusing longer
# retention, repository defaults and weaker compression. No workflow job runs.
$uploadPath = Join-Path $fixtureRoot 'MinimalUpload.yml'
[IO.File]::WriteAllText($uploadPath, $text.Replace($originalRun, $minimalUpload), [Text.UTF8Encoding]::new($false))
& $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($uploadPath)
# Infrequent serial package qualification can use GitHub's six-hour hosted-job
# ceiling. A larger value must still refuse instead of becoming unbounded.
$longJobPath = Join-Path $fixtureRoot 'HostedJobLimit.yml'
[IO.File]::WriteAllText($longJobPath, $text.Replace('    timeout-minutes: 15', '    timeout-minutes: 360'), [Text.UTF8Encoding]::new($false))
& $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($longJobPath)
foreach ($uploadControl in @(
        @{Name = 'two-day-retention'; Before = 'retention-days: 1'; After = 'retention-days: 2'; Diagnostic = 'artifact retention' },
        @{Name = 'default-retention'; Before = 'retention-days: 1'; After = 'retention-days: 0'; Diagnostic = 'artifact retention' },
        @{Name = 'missing-retention'; Before = 'retention-days: 1'; After = ''; Diagnostic = 'artifact retention' },
        @{Name = 'default-compression'; Before = 'compression-level: 9'; After = 'compression-level: 6'; Diagnostic = 'artifact compression' },
        @{Name = 'missing-compression'; Before = 'compression-level: 9'; After = ''; Diagnostic = 'artifact compression' }
    )) {
    $path = Join-Path $fixtureRoot ($uploadControl.Name + '.yml')
    [IO.File]::WriteAllText($path, $text.Replace($originalRun, $minimalUpload.Replace($uploadControl.Before, $uploadControl.After)), [Text.UTF8Encoding]::new($false))
    $message = $null
    try { & $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($path) }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($uploadControl.Diagnostic, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Policy did not reject $($uploadControl.Name) at its required boundary: $message"
    }
    Write-Output "PASS storage refusal: $($uploadControl.Name)"
}
Push-Location -LiteralPath $fixtureRoot
try { & $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @('Valid.yml') }
finally { Pop-Location }
$cases = @(
    @{Name = 'mutable-action'; Before = '3d3c42e5aac5ba805825da76410c181273ba90b1'; After = 'v7'; Diagnostic = 'approved immutable action' },
    @{Name = 'moving-runner'; Before = 'windows-2025-vs2026'; After = 'windows-latest'; Diagnostic = 'approved runner' },
    @{Name = 'privileged-pr'; Before = 'on: workflow_dispatch'; After = 'on: pull_request_target'; Diagnostic = 'pull_request_target' },
    @{Name = 'workflow-write'; Before = 'contents: read'; After = 'contents: write'; Diagnostic = 'workflow permissions' },
    @{Name = 'unbounded-job'; Before = '    timeout-minutes: 15'; After = ''; Diagnostic = 'job timeout' },
    @{Name = 'above-hosted-job-limit'; Before = '    timeout-minutes: 15'; After = '    timeout-minutes: 361'; Diagnostic = 'job timeout' },
    @{Name = 'uncancelled'; Before = 'cancel-in-progress: true'; After = 'cancel-in-progress: false'; Diagnostic = 'concurrency cancellation' },
    @{Name = 'pr-secret'; Before = 'on: workflow_dispatch'; After = "on: [workflow_dispatch, pull_request]`nenv:`n  NEVER_EXECUTED: " + '''${{ secrets.TOKEN }}'''; Diagnostic = 'manual workflow_dispatch' },
    @{Name = 'retention'; Before = "      - run: Write-Output 'Static fixture only; never executed by its validator.'"; After = "      - uses: actions/upload-artifact@cf430e030ddbb5b0abf93d22962f4752f3646cd9 # v7.0.2`n        with:`n          path: report.txt`n          retention-days: 31"; Diagnostic = 'artifact retention' },
    @{Name = 'missing-version-comment'; Before = '# v7.0.1'; After = '# selected action'; Diagnostic = 'action version comment' }
)
foreach ($case in $cases) {
    if (-not $text.Contains($case.Before, [StringComparison]::Ordinal)) { throw "Mutation fixture precondition absent: $($case.Name)" }
    $path = Join-Path $fixtureRoot ($case.Name + '.yml')
    [IO.File]::WriteAllText($path, $text.Replace($case.Before, $case.After), [Text.UTF8Encoding]::new($false))
    $message = $null
    try { & $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($path) }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.Diagnostic, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Policy did not reject $($case.Name) at its required boundary: $message"
    }
    Write-Output "PASS negative control: $($case.Name)"
}
$message = $null
try { & $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @((Join-Path $PSScriptRoot 'WorkflowFixtures/DuplicateKey.yml')) }
catch { $message = $_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Duplicate key', [StringComparison]::Ordinal)) { throw 'Duplicate YAML members were not refused natively.' }
Write-Output 'PASS: valid native workflow and all targeted policy refusals.'

# Retain actual source, then challenge SDK admission and the native compiler
# control's lane/evidence placement through the real CI contract.
$ciText = (Get-Content -LiteralPath (Join-Path $repositoryRoot '.github/workflows/continuous-integration.yml') -Raw).Replace("`r`n", "`n")
$sdkRun = '              run: ./scripts/Install-HostedWindowsSdk.ps1 -OutputDirectory artifacts/build-logs/windows-sdk'
if (-not $ciText.Contains($sdkRun, [StringComparison]::Ordinal)) { throw 'SDK setup source fixture is absent.' }
$sdkRefusal = 'Hosted SDK prerequisite must run unconditionally immediately after checkout'
$comdatRefusal = 'Native COMDAT control requires the SDK-prepared x64 Release lane and retained build-log output.'
$comdatCommand = './scripts/tests/Test-ReleaseComdatIdentityPolicy.ps1 -OwnedOutputRoot ' +
'(Join-Path $env:GITHUB_WORKSPACE ''artifacts/build-logs/x64/Release/ComdatIdentity'')'
$comdatGuard = "              if: matrix.architecture == 'x64' && matrix.configuration == 'Release'"
$runtimeCommand = './scripts/Install-HostedWindowsAppRuntime.ps1 -Architecture ''${{ matrix.architecture }}'' -OutputDirectory artifacts/build-logs/windows-app-runtime'
$runtimeRefusal = 'Hosted runtime deployment requires one bounded native x86/x64 setup after compilation and before test-host execution.'
foreach ($case in @(
        @{Name = 'omitted-sdk'; Before = $sdkRun; After = "              run: Write-Output 'SDK setup omitted'"; Expected = $sdkRefusal },
        @{Name = 'conditional-sdk'; Before = $sdkRun; After = "              if: inputs.cache_dependencies`n$sdkRun"; Expected = $sdkRefusal },
        @{Name = 'unbounded-sdk'; Before = "              timeout-minutes: $hostedPrerequisiteMinutes`n$sdkRun"; After = $sdkRun; Expected = $sdkRefusal },
        @{Name = 'comdat-unretained-diagnostics'; Before = $comdatCommand; After = './scripts/tests/Test-ReleaseComdatIdentityPolicy.ps1 -OwnedOutputRoot $env:RUNNER_TEMP'; Expected = $comdatRefusal },
        @{Name = 'comdat-wrong-lane'; Before = "$comdatGuard`n              run: $comdatCommand"; After = "              if: matrix.architecture == 'x86' && matrix.configuration == 'Release'`n              run: $comdatCommand"; Expected = $comdatRefusal },
        @{Name = 'omitted-runtime'; Before = $runtimeCommand; After = "Write-Output 'Runtime setup omitted'"; Expected = 'Hosted runtime deployment must precede unpackaged presentation test-host execution.' },
        @{Name = 'unbounded-runtime'; Before = "              timeout-minutes: $hostedPrerequisiteMinutes`n              run: $runtimeCommand"; After = "              run: $runtimeCommand"; Expected = $runtimeRefusal },
        @{Name = 'runtime-wrong-lane'; Before = "              if: matrix.architecture != 'ARM64'`n              timeout-minutes: $hostedPrerequisiteMinutes`n              run: $runtimeCommand"; After = "              if: matrix.architecture == 'ARM64'`n              timeout-minutes: $hostedPrerequisiteMinutes`n              run: $runtimeCommand"; Expected = $runtimeRefusal },
        @{Name = 'arm64-emulated-host'; Before = 'runs-on: windows-11-vs2026-arm'; After = 'runs-on: windows-2025-vs2026'; Expected = 'ARM64 runtime requires the native hosted VM' },
        @{Name = 'arm64-wrong-producer'; Before = '        needs: build'; After = '        needs: policy'; Expected = 'ARM64 runtime requires the native hosted VM' },
        @{Name = 'arm64-digest-warning'; Before = "name: arm64-headless-test-inputs`n                  path: artifacts/bin/ARM64/Release`n                  digest-mismatch: error"; After = "name: arm64-headless-test-inputs`n                  path: artifacts/bin/ARM64/Release`n                  digest-mismatch: warn"; Expected = 'ARM64 execution requires exact same-run artifact selection' },
        @{Name = 'arm64-without-manifest-fixture'; Before = "artifacts/bin/ARM64/Release/JpgSpinner.App/AppxManifest.xml`n"; After = ''; Expected = 'ARM64 test transfer must include the native manifest fixture' },
        @{Name = 'arm64-without-native-admission'; Before = '-Architecture ARM64 -RequireNativeArchitecture'; After = '-Architecture ARM64'; Expected = 'ARM64 headless tests must require actual native machine identities' },
        @{Name = 'arm64-build-on-test-host'; Before = 'run: ./scripts/Install-HostedWindowsAppRuntime.ps1 -Architecture ARM64'; After = "run: ./scripts/Invoke-Build.ps1 -Architecture ARM64`n              # ./scripts/Install-HostedWindowsAppRuntime.ps1 -Architecture ARM64"; Expected = 'ARM64 runtime job must deploy prebuilt inputs' },
        @{Name = 'arm64-not-a-package-gate'; Before = 'needs: [policy, build, arm64-runtime, asan, analysis]'; After = 'needs: [policy, build, asan, analysis]'; Expected = 'Unsigned packaging must wait for every automated qualification job.' },
        @{Name = 'exiv2-omitted-qualification'; Before = './scripts/tests/Test-Exiv2WindowsCharset.ps1'; After = "Write-Output 'Exiv2 qualification omitted' #"; Expected = 'Installed Exiv2 Windows charset qualification wiring is absent.' },
        @{Name = 'exiv2-unretained-evidence'; Before = '-EvidenceDirectory (Join-Path $PWD ''artifacts/build-logs/Exiv2Charset'')'; After = '-EvidenceDirectory (Join-Path $env:RUNNER_TEMP ''exiv2-evidence'')'; Expected = 'Installed Exiv2 charset qualification must run in every actual matrix lane with separate retained diagnostics.' }
        @{Name = 'omitted-candidate-control'; Before = './scripts/tests/Test-ReleaseCandidateArchive.ps1'; After = './scripts/tests/Absent-CandidateControl.ps1'; Expected = 'Manual CI lacks the candidate archival control' }
    )) {
    if (-not $ciText.Contains($case.Before, [StringComparison]::Ordinal)) { throw "Native CI fixture precondition differs: $($case.Name)" }
    $path = Join-Path $fixtureRoot ($case.Name + '.yml')
    [IO.File]::WriteAllText($path, $ciText.Replace($case.Before, $case.After), [Text.UTF8Encoding]::new($false))
    $message = $null
    try { & (Join-Path $PSScriptRoot 'Test-ContinuousIntegrationContract.ps1') -ToolsDirectory $ToolsDirectory -WorkflowPath $path }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains($case.Expected, [StringComparison]::Ordinal)) {
        throw "CI accepted $($case.Name) or refused it at the wrong boundary: $message"
    }
    Write-Output "PASS native CI source refusal: $($case.Name)"
}
