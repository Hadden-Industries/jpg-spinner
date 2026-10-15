<# Validate the unsigned execution record before a separate hosted job signs
   it. Native GitHub input attestation is a separate mandatory caller gate. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$EvidenceDirectory,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
    [Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,18}$')][string]$SourceRunId,
    [Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,18}$')][string]$QualificationRunId
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$directory=[IO.Path]::GetFullPath($EvidenceDirectory)
$records=@{}
foreach ($entry in @(
    @{Name='execution';File='execution.json';Schema='interactive-test-evidence.schema.json'},
    @{Name='context';File='build-context.json';Schema='interactive-test-drop.schema.json'}
)) {
    $path=Join-Path $directory $entry.File
    if ((Get-Item -LiteralPath $path).Length -gt 16384) {throw 'Interactive execution metadata exceeds its size bound.'}
    $json=[IO.File]::ReadAllText($path)
    $records[$entry.Name]=ConvertFrom-JsonWithUniqueObjectMembers -JsonText $json -SourceDescription 'Interactive execution publication metadata' -RequiredRootValueKind Object -AsHashtable
    if (-not (Test-Json -Json $json -SchemaFile (Join-Path $root "eng/$($entry.Schema)"))) {throw 'Interactive execution publication schema refused the metadata.'}
}
$evidence=$records.execution
if ($evidence.sourceCommit -cne $SourceCommit -or $records.context.sourceCommit -cne $SourceCommit -or
    $evidence.sourceRunId -cne $SourceRunId -or $evidence.qualificationRunId -cne $QualificationRunId) {
    throw 'Interactive execution publication differs from the selected candidate or runs.'
}
foreach ($entry in @(
    @{File='native.junit.xml';Digest=$evidence.junitSha256},
    @{File='build-context.json';Digest=$evidence.inputContextSha256}
)) {
    if ((Get-FileHash -LiteralPath (Join-Path $directory $entry.File)).Hash.ToLowerInvariant() -cne $entry.Digest) {
        throw 'Interactive execution publication digest differs from its native evidence.'
    }
}
# The trusted runner and successful workflow dependency supply the execution
# claim. This consumer binds the exact record/report; it does not manufacture
# independent proof of a desktop or reinterpret operator acceptance as passed.
Write-Output 'PASS: strict automated-only execution identity and native report/context digests.'
