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
foreach ($name in @('build','policy','asan','analysis')) {
    if (-not $jobs.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new($name))) { throw "Missing required hosted job: $name" }
}
$build = Get-NodeProperty $jobs 'build'
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
Write-Output 'PASS: native-validated CI source wiring and exact build/test architecture matrix; hosted execution remains unqualified.'
