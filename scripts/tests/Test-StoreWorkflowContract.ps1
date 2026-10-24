<# Native-validated protected Store source composition. This inspects YAML and
   PowerShell syntax, not live reviewer protection, hosted execution, artifact
   attestation, WACK, certification or release acceptance. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ToolsDirectory,
    [string]$WorkflowPath = (Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) '.github/workflows/store-package.yml')
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $root 'scripts/QualificationPolicy.psm1')
$hostedPrerequisiteMinutes = [string](Get-QualificationTimeout -Category hostedPrerequisiteStep -Unit Minutes)
if (-not [IO.File]::Exists($WorkflowPath)) { throw 'Protected manual Store workflow is absent.' }
& (Join-Path $root scripts/Test-GitHubWorkflowPolicy.ps1) -ToolsDirectory $ToolsDirectory -WorkflowPaths @($WorkflowPath)
$yaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
$reader = [IO.File]::OpenText($WorkflowPath)
try { $yaml.Load($reader) } finally { $reader.Dispose() }
function Get-Property($Mapping, [string]$Name) { return , $Mapping.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new($Name)] }
$workflow = $yaml.Documents[0].RootNode
$jobs = Get-Property $workflow jobs
foreach ($name in @('admission', 'package', 'attestation')) {
    if (-not $jobs.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new($name))) { throw "Store workflow lacks required job: $name" }
}
$package = Get-Property $jobs package
$sdkStep = (Get-Property $package steps).Children[1]
if ((Get-Property $sdkStep run).Value -cne './scripts/Install-HostedWindowsSdk.ps1 -OutputDirectory artifacts/build-logs/windows-sdk' -or
    (Get-Property $sdkStep 'timeout-minutes').Value -cne $hostedPrerequisiteMinutes -or $null -ne (Get-Property $sdkStep 'if')) {
    throw 'Store compiler prerequisites require unconditional bounded SDK setup immediately after checkout.'
}
if ((Get-Property $package needs).Value -cne 'admission' -or (Get-Property $package environment).Value -cne 'store-submission') {
    throw 'Store production escaped protected admission.'
}
$publisher = Get-Property $jobs attestation
if ((Get-Property $publisher needs).Value -cne 'package') { throw 'Store attestation precedes successful package qualification.' }
$commands = @{}
foreach ($entry in $jobs.Children.GetEnumerator()) {
    $name = $entry.Key.Value; $job = $entry.Value
    if ((Get-Property $job 'if').Value -cne "github.ref == 'refs/heads/main'") { throw 'Store jobs require the trusted main ref.' }
    $commands[$name] = [Collections.Generic.List[string]]::new()
    foreach ($step in (Get-Property $job steps).Children) {
        $runKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
        if ($step.Children.ContainsKey($runKey)) {
            $command = $step.Children[$runKey].Value
            $tokens = $null; $errors = $null
            [void][Management.Automation.Language.Parser]::ParseInput($command, [ref]$tokens, [ref]$errors)
            if ($errors.Count -ne 0) { throw 'Native PowerShell parser rejected a Store workflow script.' }
            $commands[$name].Add($command)
        }
        $usesKey = [YamlDotNet.RepresentationModel.YamlScalarNode]::new('uses')
        if ($step.Children.ContainsKey($usesKey) -and $step.Children[$usesKey].Value.StartsWith('actions/download-artifact@', [StringComparison]::Ordinal)) {
            if ((Get-Property (Get-Property $step with) digest-mismatch).Value -cne 'error') { throw 'Store artifact download must fail on a digest mismatch.' }
        }
    }
    foreach ($permission in (Get-Property $job permissions).Children.GetEnumerator()) {
        $pair = "$($permission.Key.Value):$($permission.Value.Value)"
        if ($name -cne 'attestation' -and $pair -cnotin @('contents:read', 'actions:read')) { throw 'Store compiler/admission tokens must remain read-only.' }
    }
}
if (-not (($commands.admission -join "`n").Contains('Test-ProtectedWorkflowAdmission.ps1 -TargetEnvironmentName store-submission', [StringComparison]::Ordinal))) { throw 'Store admission lacks the explicit current environment role.' }
$production = $commands.package -join "`n"
# This is the protected workflow's closed source-composition contract, not a
# second path parser or a claim about an unqualified host's filesystem. Keep
# C++ outputs beside the checkout: MSBuild diagnoses outputs below TEMP/TMP
# with MSB8029. Native YAML/actionlint/PowerShell validation above remains real.
$requiredStoreBuildWorkspaceSource = '$storeWorkingDirectoryPath = Join-Path (Split-Path -Parent $env:GITHUB_WORKSPACE)'
if (-not $production.Contains($requiredStoreBuildWorkspaceSource, [StringComparison]::Ordinal)) {
    throw 'Store C++ build workspace must be a sibling of the checkout, outside runner temporary storage.'
}
$previous = -1
foreach ($boundary in @('gpg.ssh.allowedSignersFile', 'Invoke-IndependentPackageBuildComparison.ps1', 'Export-JpgSpinnerVcpkgEvidence.ps1', 'New-JpgSpinnerBuildComponentEvidence.ps1', 'New-StoreSubmissionSbom.ps1')) {
    $position = $production.IndexOf($boundary, [StringComparison]::Ordinal)
    if ($position -le $previous) { throw "Store production lacks ordered boundary: $boundary" }
    $previous = $position
}
foreach ($forbidden in @('Invoke-Build.ps1', 'Invoke-IndependentPackageBuildComparison.ps1', 'New-StoreSubmissionSbom.ps1', 'go build', 'npm ci')) {
    if (($commands.attestation -join "`n").Contains($forbidden, [StringComparison]::OrdinalIgnoreCase)) { throw 'The Store attestation publisher must not compile or generate its own subject.' }
}
foreach ($forbidden in @('secrets.', 'signtool sign', 'Add-AppxPackage', 'PartnerCenter')) {
    if ([IO.File]::ReadAllText($WorkflowPath).Contains($forbidden, [StringComparison]::OrdinalIgnoreCase)) { throw "Store evidence workflow contains a non-evidence operation: $forbidden" }
}
Write-Output 'PASS: native-validated manual/protected Store composition, source verification before independent builds, exact build-component/SBOM ordering and separate read-only compiler/native-attestation roles; hosted execution remains pending.'
