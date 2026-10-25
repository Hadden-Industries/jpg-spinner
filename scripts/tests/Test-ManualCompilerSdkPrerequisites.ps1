<# Inspect compiler-lane prerequisites through the maintained native YAML model.
   This regression never executes a workflow, installer, compiler or scanner.
   The scanner lane supplies an already-correct control for the CodeQL omission. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path (Split-Path -Parent $PSScriptRoot) 'QualificationPolicy.psm1')
$prerequisiteMinutes = [string](Get-QualificationTimeout -Category hostedPrerequisiteStep -Unit Minutes)
$tools = & (Join-Path (Split-Path -Parent $PSScriptRoot) 'Install-WorkflowVerificationTools.ps1') -ToolsDirectory $ToolsDirectory
Add-Type -Path $tools.yamlAssemblyPath
$keys = @{}
foreach ($name in @('jobs', 'steps', 'run', 'if', 'timeout-minutes')) {
    $keys[$name] = [YamlDotNet.RepresentationModel.YamlScalarNode]::new($name)
}
foreach ($lane in @(
        @{ Workflow = 'scanner-fuzz.yml'; Job = 'scanner'; Diagnostics = 'artifacts/fuzz/scanner/windows-sdk' },
        @{ Workflow = 'codeql.yml'; Job = 'cpp'; Diagnostics = 'artifacts/build-logs/windows-sdk' }
    )) {
    $workflowPath = Join-Path $repositoryRoot ('.github/workflows/' + $lane.Workflow)
    $document = [YamlDotNet.RepresentationModel.YamlStream]::new()
    $reader = [IO.File]::OpenText($workflowPath)
    try { $document.Load($reader) } finally { $reader.Dispose() }
    $jobKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new($lane.Job)
    $steps = $document.Documents[0].RootNode.Children[$keys.jobs].Children[$jobKey].Children[$keys.steps]
    # Installing after CodeQL init would trace prerequisite work too. These
    # dedicated compiler lanes must prepare their SDK immediately after checkout,
    # regardless of cache/input choices, before initializing their native tools.
    $setup = $steps.Children[1]
    $command = $setup.Children[$keys.run]
    $timeout = $setup.Children[$keys['timeout-minutes']]
    $expectedCommand = './scripts/Install-HostedWindowsSdk.ps1 -OutputDirectory ' + $lane.Diagnostics
    if ($null -eq $command -or $command.Value -cne $expectedCommand -or
        $null -eq $timeout -or $timeout.Value -cne $prerequisiteMinutes -or
        $setup.Children.ContainsKey($keys['if'])) {
        throw "Manual compiler lane $($lane.Workflow)/$($lane.Job) requires unconditional bounded SDK setup immediately after checkout."
    }
    Write-Output "PASS: SDK setup precedes native tools in $($lane.Workflow)/$($lane.Job)."
}
