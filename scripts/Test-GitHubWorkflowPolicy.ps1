<# Validate syntax with consumer-maintained tooling and apply only the
   repository's approved workflow constraints; never execute workflow steps. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ToolsDirectory,
    [string[]]$WorkflowPaths = @()
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$tools = & (Join-Path $PSScriptRoot 'Install-WorkflowVerificationTools.ps1') -ToolsDirectory $ToolsDirectory
Add-Type -Path $tools.yamlAssemblyPath
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1')
$qualificationPolicy = Get-QualificationPolicy
$maximumJobMinutes = Get-QualificationTimeout -Category hostedJobLimit -Unit Minutes
$artifactPolicy = $qualificationPolicy.GetProperty('artifactPolicy')
$artifactRetentionDays = [string]([int]$artifactPolicy.GetProperty('retentionDays').GetDecimal())
$artifactCompressionLevel = [string]([int]$artifactPolicy.GetProperty('compressionLevel').GetDecimal())
$pins = ConvertFrom-JsonWithUniqueObjectMembers -JsonText (
    Get-Content -LiteralPath (Join-Path $repositoryRoot 'eng/github-actions-lock.json') -Raw
) -SourceDescription 'Approved GitHub action pins' -RequiredRootValueKind Object -AsHashtable
if ($pins.schemaVersion -ne 1) { throw 'Unsupported GitHub action-pin contract.' }

# Preserve native nodes, including scalar spelling, instead of round-tripping
# YAML through PowerShell's case-insensitive dictionaries or JSON conversion.
function Get-WorkflowProperty {
    param($Mapping, [string]$Name)
    if ($Mapping -isnot [YamlDotNet.RepresentationModel.YamlMappingNode]) { throw 'Workflow policy requires a YAML mapping.' }
    $key = [YamlDotNet.RepresentationModel.YamlScalarNode]::new($Name)
    if ($Mapping.Children.ContainsKey($key)) { return , $Mapping.Children[$key] }
    return $null
}
function Get-WorkflowScalar {
    param($Node)
    if ($Node -isnot [YamlDotNet.RepresentationModel.YamlScalarNode]) { return $null }
    return $Node.Value
}
if ($WorkflowPaths.Count -eq 0) {
    $workflowDirectory = Join-Path $repositoryRoot '.github/workflows'
    foreach ($required in @('continuous-integration.yml', 'codeql.yml', 'scanner-fuzz.yml', 'store-package.yml', 'interactive-qualification.yml')) {
        if (-not (Test-Path -LiteralPath (Join-Path $workflowDirectory $required) -PathType Leaf)) { throw "Required workflow is absent: $required" }
    }
    $WorkflowPaths = @(Get-ChildItem -LiteralPath $workflowDirectory -File | Where-Object Extension -In @('.yml', '.yaml') | Select-Object -ExpandProperty FullName)
}
foreach ($path in $WorkflowPaths) {
    # Bind relative inputs once, before the child-process working-directory
    # boundary. Native syntax and project policy must inspect identical bytes.
    $workflowPath = Convert-Path -LiteralPath $path
    if ((Get-Item -LiteralPath $workflowPath).Length -gt 262144) { throw 'Workflow exceeds the 256 KiB policy input limit.' }
    $text = [IO.File]::ReadAllText($workflowPath)
    $yaml = [YamlDotNet.RepresentationModel.YamlStream]::new()
    $reader = [IO.StringReader]::new($text)
    try { $yaml.Load($reader) } finally { $reader.Dispose() }
    if ($yaml.Documents.Count -ne 1 -or $yaml.Documents[0].RootNode -isnot [YamlDotNet.RepresentationModel.YamlMappingNode]) {
        throw 'Workflow must have exactly one YAML object root.'
    }
    $workflow = $yaml.Documents[0].RootNode
    $isStoreWorkflow = (Get-WorkflowScalar (Get-WorkflowProperty $workflow 'name')) -ceq 'Store submission evidence'
    $permissions = Get-WorkflowProperty $workflow 'permissions'
    if ($permissions -isnot [YamlDotNet.RepresentationModel.YamlMappingNode] -or $permissions.Children.Count -ne 1 -or
        (Get-WorkflowScalar (Get-WorkflowProperty $permissions 'contents')) -cne 'read') { throw 'Require explicit read-only workflow permissions (contents: read).' }
    $concurrency = Get-WorkflowProperty $workflow 'concurrency'
    # Store archival replaces the previous candidate only after successor byte
    # readback. Cancellation could strand that transition; serialize it instead.
    $expectedCancellation = if ($isStoreWorkflow) { 'false' } else { 'true' }
    if ((Get-WorkflowScalar (Get-WorkflowProperty $concurrency 'cancel-in-progress')) -cne $expectedCancellation -or
        [string]::IsNullOrWhiteSpace((Get-WorkflowScalar (Get-WorkflowProperty $concurrency 'group')))) { throw 'Require the workflow-specific concurrency cancellation policy and group.' }
    $trigger = Get-WorkflowProperty $workflow 'on'
    $events = switch ($trigger.NodeType.ToString()) {
        'Scalar' { $trigger.Value }
        'Sequence' { foreach ($eventNode in $trigger.Children) { Get-WorkflowScalar $eventNode } }
        'Mapping' { foreach ($entry in $trigger.Children.GetEnumerator()) { Get-WorkflowScalar $entry.Key } }
        default { throw 'Unsupported workflow trigger representation.' }
    }
    if ('pull_request_target' -cin $events) { throw 'Privileged pull_request_target execution is forbidden.' }
    # Explicit user policy: only a deliberate UI/CLI/API workflow_dispatch may
    # consume remote runners. Reject mixed triggers and reusable workflow_call
    # entry points too; neither a push nor another workflow may start these jobs.
    # Native YAML nodes above preserve all GitHub-supported trigger forms.
    if (@($events).Count -ne 1 -or @($events)[0] -cne 'workflow_dispatch') {
        throw 'Remote execution requires only manual workflow_dispatch; automatic or mixed workflow triggers are forbidden.'
    }
    # The native parser exposes comments as events. Use those events to check
    # version annotations, rather than interpreting YAML comments ourselves.
    $comments = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $reader = [IO.StringReader]::new($text)
    try {
        $scanner = [YamlDotNet.Core.Scanner]::new($reader, $false)
        $parser = [YamlDotNet.Core.Parser]::new($scanner)
        while ($parser.MoveNext()) {
            if ($parser.Current -is [YamlDotNet.Core.Events.Comment]) {
                [void]$comments.Add("$($parser.Current.Start.Line):$($parser.Current.Value.Trim())")
            }
        }
    }
    finally { $reader.Dispose() }
    $jobs = Get-WorkflowProperty $workflow 'jobs'
    if ($jobs -isnot [YamlDotNet.RepresentationModel.YamlMappingNode] -or $jobs.Children.Count -eq 0) { throw 'Workflow requires actual jobs.' }
    foreach ($jobEntry in $jobs.Children.GetEnumerator()) {
        $job = $jobEntry.Value
        $runner = Get-WorkflowProperty $job 'runs-on'
        $isInteractive = $runner -is [YamlDotNet.RepresentationModel.YamlSequenceNode] -and
        ((@($runner.Children | ForEach-Object Value) -join ',') -ceq 'self-hosted,Windows,X64,jpg-spinner-interactive')
        if ((Get-WorkflowScalar $runner) -cnotin @('windows-2025-vs2026', 'windows-11-vs2026-arm') -and -not $isInteractive) { throw 'Job must select an approved runner.' }
        if ($isInteractive -and $null -eq (Get-WorkflowProperty $job 'environment')) { throw 'Interactive runner requires a protected manual qualification job.' }
        $timeoutText = Get-WorkflowScalar (Get-WorkflowProperty $job 'timeout-minutes')
        $timeout = 0
        # GitHub-hosted jobs have a six-hour execution ceiling. Serial release
        # qualification may use it; missing, dynamic or larger bounds refuse.
        # https://docs.github.com/actions/reference/workflows-and-actions/workflow-syntax#jobsjob_idtimeout-minutes
        if (-not [int]::TryParse($timeoutText, [ref]$timeout) -or $timeout -lt 1 -or $timeout -gt $maximumJobMinutes) { throw "Require a bounded literal job timeout between 1 and $maximumJobMinutes minutes." }
        $jobPermissions = Get-WorkflowProperty $job 'permissions'
        $steps = Get-WorkflowProperty $job 'steps'
        if ($steps -isnot [YamlDotNet.RepresentationModel.YamlSequenceNode]) { throw 'Job must declare actual steps.' }
        $hasNativeAttestation = $false
        foreach ($step in $steps.Children) {
            $uses = Get-WorkflowScalar (Get-WorkflowProperty $step 'uses')
            if ($uses -ceq "actions/attest@$($pins.actions['actions/attest'].commit)") { $hasNativeAttestation = $true }
        }
        if ($null -ne $jobPermissions) {
            if ($jobPermissions -isnot [YamlDotNet.RepresentationModel.YamlMappingNode]) { throw 'Job permissions must be an explicit mapping.' }
            foreach ($permission in $jobPermissions.Children.GetEnumerator()) {
                $pair = "$(Get-WorkflowScalar $permission.Key):$(Get-WorkflowScalar $permission.Value)"
                if ($pair -cin @('attestations:write', 'id-token:write')) {
                    # Only a trusted-main hosted publisher can request platform
                    # signing. Compilers and self-hosted desktop jobs retain
                    # read-only tokens; the signing job has no product secrets.
                    if (-not $hasNativeAttestation -or $isInteractive -or
                        (Get-WorkflowScalar (Get-WorkflowProperty $job 'if')) -cne "github.ref == 'refs/heads/main'") {
                        throw 'Platform attestation permissions require a trusted-main hosted native publisher.'
                    }
                }
                elseif ($pair -ceq 'contents:write') {
                    # Reserve repository writes for this isolated archival role.
                    # Its stronger source-composition contract separately rejects
                    # compilation, signing and an incomplete predecessor chain.
                    if (-not $isStoreWorkflow -or $jobEntry.Key.Value -cne 'archive-candidate' -or $isInteractive -or
                        $hasNativeAttestation -or (Get-WorkflowScalar (Get-WorkflowProperty $job 'needs')) -cne 'attestation' -or
                        (Get-WorkflowScalar (Get-WorkflowProperty $job 'if')) -cne "github.ref == 'refs/heads/main'") {
                        throw 'Repository write permission requires the isolated trusted-main Store archival role after attestation.'
                    }
                }
                elseif ($pair -cnotin @('contents:read', 'actions:read', 'security-events:write')) { throw 'Job requests unapproved permissions.' }
            }
        }
        foreach ($step in $steps.Children) {
            $usesNode = Get-WorkflowProperty $step 'uses'
            if ($null -eq $usesNode) { continue }
            $uses = Get-WorkflowScalar $usesNode
            if ($uses -cnotmatch '^(?<action>[A-Za-z0-9_./-]+)@(?<commit>[a-f0-9]{40})$') { throw 'Step must reference an approved immutable action commit.' }
            $actionName = $Matches.action
            $commit = $Matches.commit
            $ownerAction = if ($actionName -cin @('github/codeql-action/init', 'github/codeql-action/analyze')) { 'github/codeql-action' } else { $actionName }
            if (-not $pins.actions.ContainsKey($ownerAction) -or $pins.actions[$ownerAction].commit -cne $commit) { throw 'Step differs from the approved immutable action commit.' }
            if (-not $comments.Contains("$($usesNode.End.Line):v$($pins.actions[$ownerAction].version)")) { throw 'Step lacks its exact native action version comment.' }
            $arguments = Get-WorkflowProperty $step 'with'
            if ($actionName -ceq 'actions/checkout' -and
                (Get-WorkflowScalar (Get-WorkflowProperty $arguments 'persist-credentials')) -cne 'false') { throw 'Checkout must disable persisted credentials.' }
            if ($actionName -ceq 'actions/upload-artifact') {
                $retentionText = Get-WorkflowScalar (Get-WorkflowProperty $arguments 'retention-days')
                # Cloud artifacts are short-lived transport/diagnostics. Export
                # release evidence before expiry; Actions is not its archive.
                if ($retentionText -cne $artifactRetentionDays) { throw "Require literal artifact retention of exactly $artifactRetentionDays day(s)." }
                if ((Get-WorkflowScalar (Get-WorkflowProperty $arguments 'compression-level')) -cne $artifactCompressionLevel) {
                    throw "Require literal artifact compression level $artifactCompressionLevel to minimize shared Actions storage."
                }
            }
        }
    }
    Push-Location -LiteralPath $repositoryRoot
    try {
        # Pin native configuration for external refusal fixtures too: actionlint
        # otherwise discovers configuration from each input's directory instead
        # of this repository's approved label declaration.
        & $tools.actionlintExecutablePath -color=false -config-file (Join-Path $repositoryRoot '.github/actionlint.yaml') $workflowPath
        if ($LASTEXITCODE -ne 0) { throw 'Native actionlint rejected workflow syntax or expressions.' }
    }
    finally { Pop-Location }
    Write-Output "PASS: native GitHub workflow syntax and repository policy: $path"
}
