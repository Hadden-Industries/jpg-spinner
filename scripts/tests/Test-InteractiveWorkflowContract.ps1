<# Source contract only; native actionlint and YAML validation run before the
   residual checks. This cannot establish a deployed, unlocked VM or UI pass. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$ToolsDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$path=Join-Path $root '.github/workflows/interactive-qualification.yml'
& (Join-Path $root 'scripts/Test-GitHubWorkflowPolicy.ps1') -ToolsDirectory $ToolsDirectory -WorkflowPaths @($path)
$yaml=[YamlDotNet.RepresentationModel.YamlStream]::new()
$reader=[IO.File]::OpenText($path)
try {$yaml.Load($reader)} finally {$reader.Dispose()}
function Get-Property($Mapping,[string]$Name) {return ,$Mapping.Children[[YamlDotNet.RepresentationModel.YamlScalarNode]::new($Name)]}
$jobs=Get-Property $yaml.Documents[0].RootNode 'jobs'
if ((Get-Property (Get-Property (Get-Property $yaml.Documents[0].RootNode 'defaults') 'run') 'shell').Value -cne 'pwsh') {throw 'Interactive script syntax validation requires the actual pwsh default.'}
foreach ($name in @('admission','qualification','attestation')) {
    if (-not $jobs.Children.ContainsKey([YamlDotNet.RepresentationModel.YamlScalarNode]::new($name))) {throw "Interactive workflow lacks required job: $name"}
}
$qualification=Get-Property $jobs 'qualification'
if (((Get-Property $qualification 'runs-on').Children.Value -join ',') -cne 'self-hosted,Windows,X64,jpg-spinner-interactive' -or
    (Get-Property $qualification 'environment').Value -cne 'interactive-qualification' -or
    (Get-Property $qualification 'needs').Value -cne 'admission') {throw 'Interactive execution escaped its protected dedicated lane.'}
$permissions=Get-Property $qualification 'permissions'
foreach ($entry in $permissions.Children.GetEnumerator()) {
    if ("$($entry.Key.Value):$($entry.Value.Value)" -cnotin @('contents:read','actions:read')) {throw 'Desktop token is not read-only.'}
}
$commands=[Collections.Generic.List[string]]::new()
foreach ($job in $jobs.Children.Values) {
    foreach ($step in (Get-Property $job 'steps').Children) {
        $runKey=[YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
        if ($step.Children.ContainsKey($runKey)) {
            $command=$step.Children[$runKey].Value
            $tokens=$null;$parseErrors=$null
            [void][Management.Automation.Language.Parser]::ParseInput($command,[ref]$tokens,[ref]$parseErrors)
            if ($parseErrors.Count -ne 0) {throw 'Native PowerShell parser rejected an interactive workflow script.'}
            $commands.Add($command)
        }
    }
}
$commandsText=$commands -join "`n"
foreach ($required in @('Test-InteractiveWorkflowAdmission.ps1','gh attestation verify','--source-digest','--source-ref refs/heads/main',
    '--signer-workflow Hadden-Industries/jpg-spinner/.github/workflows/continuous-integration.yml','--deny-self-hosted-runners',
    'Test-InteractiveTestDrop.ps1','Invoke-TestSuite.ps1','-ExecutionEnvironment Interactive',
    "'[ui-automation]~[operator-assisted]'",'New-InteractiveTestEvidence.ps1')) {
    if (-not $commandsText.Contains($required,[StringComparison]::Ordinal)) {throw "Interactive workflow lacks boundary: $required"}
}
$desktopCommands=[Collections.Generic.List[string]]::new()
foreach ($step in (Get-Property $qualification 'steps').Children) {
    $runKey=[YamlDotNet.RepresentationModel.YamlScalarNode]::new('run')
    if ($step.Children.ContainsKey($runKey)) {$desktopCommands.Add($step.Children[$runKey].Value)}
}
$desktopText=$desktopCommands -join "`n"
$previous=-1
foreach ($boundary in @('gh attestation verify','Test-InteractiveTestDrop.ps1','Invoke-TestSuite.ps1','New-InteractiveTestEvidence.ps1')) {
    $position=$desktopText.IndexOf($boundary,[StringComparison]::Ordinal)
    if ($position -le $previous) {throw 'Desktop execution precedes input authentication or result generation precedes tests.'}
    $previous=$position
}
$signer=Get-Property $jobs 'attestation'
if ((Get-Property $signer 'needs').Value -cne 'qualification') {throw 'Execution signing must depend on successful desktop qualification.'}
foreach ($forbidden in @('Invoke-Build.ps1','msbuild','dotnet build','vcpkg install','signtool','secrets.')) {
    if ([IO.File]::ReadAllText($path).Contains($forbidden,[StringComparison]::OrdinalIgnoreCase)) {throw "Deployment-only workflow contains forbidden operation: $forbidden"}
}
Write-Output 'PASS: native-validated deployment-only source contract; hosted execution remains pending.'
