<# Mutate one independently authored valid workflow per case. Each check
   invokes the actual policy and native validators, without running jobs. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory, [Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
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

# Retain the actual source as each fixture, then alter only SDK admission. The
# real CI contract must refuse omission, conditional setup and unbounded setup.
$ciText = (Get-Content -LiteralPath (Join-Path $repositoryRoot '.github/workflows/continuous-integration.yml') -Raw).Replace("`r`n", "`n")
$sdkRun = '              run: ./scripts/Install-HostedWindowsSdk.ps1 -OutputDirectory artifacts/build-logs/windows-sdk'
if (-not $ciText.Contains($sdkRun, [StringComparison]::Ordinal)) { throw 'SDK setup source fixture is absent.' }
foreach ($case in @(
    @{Name = 'omitted-sdk'; Before = $sdkRun; After = "              run: Write-Output 'SDK setup omitted'" },
    @{Name = 'conditional-sdk'; Before = $sdkRun; After = "              if: inputs.cache_dependencies`n$sdkRun" },
    @{Name = 'unbounded-sdk'; Before = "              timeout-minutes: 15`n$sdkRun"; After = $sdkRun }
)) {
    if (-not $ciText.Contains($case.Before, [StringComparison]::Ordinal)) { throw "SDK fixture precondition differs: $($case.Name)" }
    $path = Join-Path $fixtureRoot ($case.Name + '.yml')
    [IO.File]::WriteAllText($path, $ciText.Replace($case.Before, $case.After), [Text.UTF8Encoding]::new($false))
    $message = $null
    try { & (Join-Path $PSScriptRoot 'Test-ContinuousIntegrationContract.ps1') -ToolsDirectory $ToolsDirectory -WorkflowPath $path }
    catch { $message = $_.Exception.Message }
    if ($null -eq $message -or -not $message.Contains('Hosted SDK prerequisite must run unconditionally immediately after checkout', [StringComparison]::Ordinal)) {
        throw "CI accepted $($case.Name) or refused it at the wrong boundary: $message"
    }
    Write-Output "PASS hosted SDK source refusal: $($case.Name)"
}
