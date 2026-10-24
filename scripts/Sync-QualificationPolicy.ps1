<# Materialize policy values required by consumers that cannot read repository
   JSON at runtime: GitHub job/step declarations and C++ test constants. YAML
   structure and scalar boundaries come from YamlDotNet, preserving comments
   and all unrelated source bytes. Default Check mode never writes files. #>
[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$ToolsDirectory,
    [ValidateSet('Check', 'Write')][string]$Mode = 'Check',
    [string]$RepositoryRoot = (Split-Path -Parent $PSScriptRoot)
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1')
$policy = Read-QualificationPolicy -Path (Join-Path $RepositoryRoot 'eng/qualification-policy.json')
$tools = & (Join-Path $PSScriptRoot 'Install-WorkflowVerificationTools.ps1') -ToolsDirectory $ToolsDirectory
Add-Type -Path $tools.yamlAssemblyPath
$keys = @{}
foreach ($name in @('jobs', 'steps', 'name', 'uses', 'with', 'timeout-minutes', 'retention-days', 'compression-level')) {
    $keys[$name] = [YamlDotNet.RepresentationModel.YamlScalarNode]::new($name)
}

function Add-PolicyScalarEdit {
    <# Collect only a native scalar's exact source span. Missing/non-scalar
       structure is an authoring error, not a reason to rewrite a workflow. #>
    param($Node, [int]$Expected, $Edits)
    if ($Node -isnot [YamlDotNet.RepresentationModel.YamlScalarNode]) { throw 'Qualification policy requires an existing scalar consumer.' }
    if ($Node.Value -cne [string]$Expected -or $Node.Style -ne [YamlDotNet.Core.ScalarStyle]::Plain) {
        $Edits.Add([pscustomobject]@{start = $Node.Start.Index; length = $Node.End.Index - $Node.Start.Index; value = [string]$Expected })
    }
}

$seenSteps = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
$stepBindings = $policy.GetProperty('workflowStepCategories')
$artifactPolicy = $policy.GetProperty('artifactPolicy')
foreach ($binding in $policy.GetProperty('workflowJobCategories').EnumerateObject()) {
    # The schema restricts each binding to a plain workflow basename, so this
    # cannot escape the selected repository into another authoring directory.
    $path = Join-Path $RepositoryRoot ('.github/workflows/' + $binding.Name)
    $source = [IO.File]::ReadAllText($path)
    $yaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
    $reader = [IO.StringReader]::new($source)
    try { $yaml.Load($reader) } finally { $reader.Dispose() }
    if ($yaml.Documents.Count -ne 1) { throw 'Qualification policy requires one native workflow document.' }
    $jobs = $yaml.Documents[0].RootNode.Children[$keys.jobs]
    if ($jobs.Children.Count -ne @($binding.Value.EnumerateObject()).Count) { throw "Qualification workflow jobs do not match the complete policy binding: $($binding.Name)" }
    $edits = [Collections.Generic.List[object]]::new()
    foreach ($jobBinding in $binding.Value.EnumerateObject()) {
        $job = $jobs.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new($jobBinding.Name)]
        $minutes = Get-QualificationTimeout -Policy $policy -Category $jobBinding.Value.GetString() -Unit Minutes
        Add-PolicyScalarEdit -Node $job.Children[$keys['timeout-minutes']] -Expected $minutes -Edits $edits
        foreach ($step in $job.Children[$keys.steps].Children) {
            if ($step.Children.ContainsKey($keys.name)) {
                $category = [Text.Json.JsonElement]::new()
                $stepName = $step.Children[$keys.name].Value
                if ($stepBindings.TryGetProperty($stepName, [ref]$category)) {
                    [void]$seenSteps.Add($stepName)
                    $minutes = Get-QualificationTimeout -Policy $policy -Category $category.GetString() -Unit Minutes
                    Add-PolicyScalarEdit -Node $step.Children[$keys['timeout-minutes']] -Expected $minutes -Edits $edits
                }
            }
            if ($step.Children.ContainsKey($keys.uses) -and $step.Children[$keys.uses].Value.StartsWith('actions/upload-artifact@', [StringComparison]::Ordinal)) {
                $arguments = $step.Children[$keys.with]
                Add-PolicyScalarEdit -Node $arguments.Children[$keys['retention-days']] -Expected ([int]$artifactPolicy.GetProperty('retentionDays').GetDecimal()) -Edits $edits
                Add-PolicyScalarEdit -Node $arguments.Children[$keys['compression-level']] -Expected ([int]$artifactPolicy.GetProperty('compressionLevel').GetDecimal()) -Edits $edits
            }
        }
    }
    if ($edits.Count -eq 0) { continue }
    if ($Mode -ceq 'Check') { throw "Generated qualification policy values are stale: $($binding.Name)" }
    foreach ($edit in ($edits | Sort-Object start -Descending)) {
        $source = $source.Remove($edit.start, $edit.length).Insert($edit.start, $edit.value)
    }
    if ($PSCmdlet.ShouldProcess($path, 'Materialize canonical qualification policy values')) {
        [IO.File]::WriteAllText($path, $source, [Text.UTF8Encoding]::new($false))
    }
}
foreach ($binding in $stepBindings.EnumerateObject()) {
    if (-not $seenSteps.Contains($binding.Name)) { throw "Qualification step binding has no consumer: $($binding.Name)" }
}

$header = [Collections.Generic.List[string]]::new()
$header.Add('// Generated by scripts/Sync-QualificationPolicy.ps1 from eng/qualification-policy.json.')
$header.Add('// Edit the category policy, then regenerate; these values are not another authority.')
$header.Add('#pragma once')
$header.Add('#include <chrono>')
$header.Add('')
$header.Add('namespace jpg_spinner::test_support::qualification_timing')
$header.Add('{')
$cppNames = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($category in $policy.GetProperty('cppTimeoutCategories').EnumerateArray()) {
    $name = $category.GetString()
    if (-not $cppNames.Add($name)) { throw 'Duplicate generated C++ qualification identifier.' }
    $milliseconds = Get-QualificationTimeout -Policy $policy -Category $name -Unit Milliseconds
    $header.Add("inline constexpr std::chrono::milliseconds $name{$milliseconds};")
}
foreach ($polling in $policy.GetProperty('cppPollingMilliseconds').EnumerateObject()) {
    if (-not $cppNames.Add($polling.Name)) { throw 'Duplicate generated C++ qualification identifier.' }
    $milliseconds = [int]$polling.Value.GetDecimal()
    $header.Add("inline constexpr std::chrono::milliseconds $($polling.Name){$milliseconds};")
}
$header.Add('} // namespace jpg_spinner::test_support::qualification_timing')
$headerText = ($header -join "`n") + "`n"
$headerPath = Join-Path $RepositoryRoot 'tests/JpgSpinner.Presentation.Tests/QualificationTiming.h'
if (-not [IO.File]::Exists($headerPath) -or [IO.File]::ReadAllText($headerPath) -cne $headerText) {
    if ($Mode -ceq 'Check') { throw 'Generated qualification timing header is stale.' }
    if ($PSCmdlet.ShouldProcess($headerPath, 'Materialize native C++ qualification durations')) {
        [IO.File]::WriteAllText($headerPath, $headerText, [Text.UTF8Encoding]::new($false))
    }
}
Write-Output "PASS: qualification policy materializations ($Mode)."
