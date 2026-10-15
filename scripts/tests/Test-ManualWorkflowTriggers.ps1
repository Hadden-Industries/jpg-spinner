<# The user's remote execution policy is manual-only for every workflow, not
   merely the current CI filename. Reuse native YAML/action validation through
   the owning policy; never execute the controlled workflow fixtures. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory,[Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$policy=Join-Path $root 'scripts/Test-GitHubWorkflowPolicy.ps1'
$text=Get-Content (Join-Path $PSScriptRoot 'WorkflowFixtures/Valid.yml') -Raw
if (-not $text.Contains('on: workflow_dispatch',[StringComparison]::Ordinal)) { throw 'Manual trigger fixture precondition differs.' }
$output=Join-Path $OwnedOutputRoot ('manual-workflow-triggers-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($output)
# GitHub accepts scalar, sequence and mapping forms. All must admit exactly
# workflow_dispatch; adding a second event must not preserve admission.
$cases=@(
    @{Name='manual-scalar';Trigger='on: workflow_dispatch';Accepted=$true},
    @{Name='manual-sequence';Trigger='on: [workflow_dispatch]';Accepted=$true},
    @{Name='manual-mapping';Trigger="on:`n  workflow_dispatch:";Accepted=$true},
    @{Name='push';Trigger='on: push';Accepted=$false},
    @{Name='pull-request';Trigger='on: pull_request';Accepted=$false},
    @{Name='release';Trigger='on: release';Accepted=$false},
    @{Name='reusable-call';Trigger='on: workflow_call';Accepted=$false},
    @{Name='mixed-sequence';Trigger='on: [workflow_dispatch, push]';Accepted=$false},
    @{Name='mixed-mapping';Trigger="on:`n  workflow_dispatch:`n  push:";Accepted=$false},
    @{Name='scheduled';Trigger="on:`n  schedule:`n    - cron: '43 1 * * *'";Accepted=$false}
)
foreach($case in $cases){
    $path=Join-Path $output ($case.Name+'.yml')
    [IO.File]::WriteAllText($path,$text.Replace('on: workflow_dispatch',$case.Trigger),[Text.UTF8Encoding]::new($false))
    $refusal=$null
    try { & $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths @($path) }
    catch { $refusal=$_.Exception.Message }
    if($case.Accepted){
        if($null -ne $refusal){throw "Native manual trigger form was refused: $($case.Name): $refusal"}
    } elseif($null -eq $refusal -or -not $refusal.Contains('manual workflow_dispatch',[StringComparison]::Ordinal)){
        throw "Automatic trigger was not refused at the manual-only boundary: $($case.Name): $refusal"
    }
    Write-Output "PASS trigger control: $($case.Name)"
}
$workflows=@(Get-ChildItem -LiteralPath (Join-Path $root '.github/workflows') -File |
    Where-Object Extension -In @('.yml','.yaml') | Select-Object -ExpandProperty FullName)
if($workflows.Count -eq 0){throw 'No actual workflows were discovered.'}
& $policy -ToolsDirectory $ToolsDirectory -WorkflowPaths $workflows
Write-Output 'PASS: every current remote workflow and all trigger forms enforce manual-only execution.'
