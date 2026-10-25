<# Challenge the actual protected workflow source in owned copies. Native YAML,
   actionlint and PowerShell parsing remain real; no workflow job is executed. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory, [Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$contract = Join-Path $PSScriptRoot Test-StoreWorkflowContract.ps1
$source = [IO.File]::ReadAllText((Join-Path $root .github/workflows/store-package.yml)).Replace("`r`n", "`n")
& $contract -ToolsDirectory $ToolsDirectory
$fixture = Join-Path $OwnedOutputRoot ('store-workflow-controls-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$cases = @(
    @{Name = 'wrong-environment'; Before = 'environment: store-submission'; After = 'environment: interactive-qualification'; Expected = 'escaped protected admission' },
    @{Name = 'wrong-admission-role'; Before = '-TargetEnvironmentName store-submission'; After = '-TargetEnvironmentName interactive-qualification'; Expected = 'explicit current environment role' },
    @{Name = 'permissive-download'; Before = 'digest-mismatch: error'; After = 'digest-mismatch: warn'; Expected = 'fail on a digest mismatch' },
    @{Name = 'signer-before-producer'; Before = 'needs: package'; After = 'needs: admission'; Expected = 'precedes successful package qualification' },
    @{Name = 'missing-comparison'; Before = 'Invoke-IndependentPackageBuildComparison.ps1'; After = 'Unqualified-PackageComparison.ps1'; Expected = 'ordered boundary' },
    @{Name = 'missing-candidate-context'; Before = 'candidate-context.json'; After = 'missing-context.json'; Expected = 'ordered boundary' },
    @{Name = 'missing-candidate-membership'; Before = 'New-PayloadManifest.ps1'; After = 'Missing-PayloadManifest.ps1'; Expected = 'ordered boundary' },
    @{Name = 'temporary-native-build-workspace'; Before = '(Split-Path -Parent $env:GITHUB_WORKSPACE)'; After = '$env:RUNNER_TEMP'; Expected = 'workspace must be a sibling' },
    @{Name = 'nested-checkout-build-workspace'; Before = '(Split-Path -Parent $env:GITHUB_WORKSPACE)'; After = '$env:GITHUB_WORKSPACE'; Expected = 'workspace must be a sibling' },
    @{Name = 'privileged-compiler'; YamlMutation = 'compiler-id-token-write'; Expected = 'trusted-main hosted native publisher' },
    @{Name = 'compiler-in-publisher'; YamlMutation = 'publisher-compile-step'; Expected = 'publisher must not compile' },
    @{Name = 'cancellable-candidate-replacement'; Before = 'cancel-in-progress: false'; After = 'cancel-in-progress: true'; Expected = 'workflow-specific concurrency' },
    @{Name = 'uncoordinated-candidate-group'; Before = 'group: ${{ github.workflow }}-${{ github.ref }}'; After = 'group: ${{ github.workflow }}-${{ github.run_id }}'; Expected = 'serialized workflow/ref group' },
    @{Name = 'archive-before-attestation'; Before = 'needs: attestation'; After = 'needs: package'; Expected = 'archival role after attestation' },
    @{Name = 'archive-compiler'; Before = './scripts/Update-ReleaseCandidateArchive.ps1 -WorkingDirectory $candidateArchiveRoot'; After = './scripts/Invoke-Build.ps1'; Expected = 'archive entry point and diagnostics' },
    @{Name = 'privileged-archive'; YamlMutation = 'archive-id-token-write'; Expected = 'trusted-main hosted native publisher' },
    @{Name = 'archive-extra-permission'; YamlMutation = 'archive-security-events-write'; Expected = 'only contents write and Actions read' },
    @{Name = 'duplicate-archive-diagnostics'; Before = '${{ env.CANDIDATE_ARCHIVE_ROOT }}/logs/'; After = '${{ env.CANDIDATE_ARCHIVE_ROOT }}/'; Expected = 'without duplicating the archive ZIP' }
)
foreach ($case in $cases) {
    $path = Join-Path $fixture ($case.Name + '.yml')
    if ($case.ContainsKey('YamlMutation')) {
        # Native node marks locate the two block insertions without assuming
        # indentation. Preserve the remaining source verbatim: representation-
        # model serialization drops the required action-version comments.
        # YamlDotNet marks use zero-based Index and one-based Column here.
        $yaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
        $reader = [IO.StringReader]::new($source)
        try { $yaml.Load($reader) } finally { $reader.Dispose() }
        $jobs = $yaml.Documents[0].RootNode.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('jobs')]
        switch ($case.YamlMutation) {
            'compiler-id-token-write' {
                $compiler = $jobs.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('package')]
                $permissions = $compiler.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('permissions')]
                if ($permissions.Style -ne [YamlDotNet.Core.Events.MappingStyle]::Block) {
                    throw 'Compiler permission fixture requires the actual block mapping.'
                }
                $insertionNode = $permissions
                $insertionSource = "id-token: write`n"
            }
            'publisher-compile-step' {
                $publisher = $jobs.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('attestation')]
                $steps = $publisher.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('steps')]
                if ($steps.Style -ne [YamlDotNet.Core.Events.SequenceStyle]::Block) {
                    throw 'Publisher compile-step fixture requires the actual block sequence.'
                }
                $insertionNode = $steps
                $insertionSource = "- run: ./scripts/New-StoreSubmissionSbom.ps1`n"
            }
            { $_ -cin @('archive-id-token-write', 'archive-security-events-write') } {
                $archiver = $jobs.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('archive-candidate')]
                $insertionNode = $archiver.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new('permissions')]
                $insertionSource = if ($case.YamlMutation -ceq 'archive-id-token-write') { "id-token: write`n" } else { "security-events: write`n" }
            }
            default { throw "Unknown native YAML control mutation: $($case.YamlMutation)" }
        }
        $insertionSource += ' ' * ([int]$insertionNode.Start.Column - 1)
        $mutatedSource = $source.Insert([int]$insertionNode.Start.Index, $insertionSource)
        [IO.File]::WriteAllText($path, $mutatedSource, [Text.UTF8Encoding]::new($false))
    }
    else {
        if (-not $source.Contains($case.Before, [StringComparison]::Ordinal)) { throw "Store control lacks its actual source precondition: $($case.Name)" }
        [IO.File]::WriteAllText($path, $source.Replace($case.Before, $case.After), [Text.UTF8Encoding]::new($false))
    }
    $refusal = $null
    try { & $contract -ToolsDirectory $ToolsDirectory -WorkflowPath $path | Out-Null } catch { $refusal = $_.Exception.Message }
    if ($null -eq $refusal -or -not $refusal.Contains($case.Expected, [StringComparison]::Ordinal)) { throw "Store control was not refused at its intended boundary: $($case.Name): $refusal" }
    Write-Output "PASS: Store source refusal: $($case.Name)"
}
