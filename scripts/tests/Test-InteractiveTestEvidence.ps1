<# Test the native producer format, not a universal JUnit grammar. Catch2's
   suite tests attribute counts assertions; testcase elements can be sections. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$producer=Join-Path $root 'scripts/New-InteractiveTestEvidence.ps1'
$fixture=Join-Path $OwnedOutputRoot ('interactive-evidence-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
$context=Join-Path $fixture 'build-context.json'
[IO.File]::WriteAllText($context,('{"schemaVersion":1,"sourceCommit":"'+('1'*40)+'","architecture":"x64","configuration":"Release","payloadManifestSha256":"'+('2'*64)+'"}'))
$report=Join-Path $fixture 'native.junit.xml'
$started=[DateTimeOffset]::UtcNow.AddSeconds(-2)
$timestamp=[DateTimeOffset]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
$xml='<testsuites><testsuite name="JpgSpinner.Presentation.Tests" tests="27" errors="0" failures="0" skipped="0" timestamp="'+$timestamp+'"><testcase name="owned passing case" status="run" /></testsuite></testsuites>'
[IO.File]::WriteAllText($report,$xml)
$output=Join-Path $fixture 'execution.json'
& $producer -JUnitPath $report -InputContextPath $context -SourceCommit ('1'*40) -SourceRunId 42 -QualificationRunId 43 -StartedAt $started -OutputPath $output | Out-Null
$evidence=Get-Content -LiteralPath $output -Raw | ConvertFrom-Json
if ($evidence.assertionCount -ne 27 -or $evidence.reportedCaseCount -ne 1 -or $evidence.scope -cne 'automated-ui-only' -or
    $evidence.junitSha256 -cne (Get-FileHash -LiteralPath $report).Hash.ToLowerInvariant() -or
    $evidence.inputContextSha256 -cne (Get-FileHash -LiteralPath $context).Hash.ToLowerInvariant()) {throw 'Native result counts, scope or input/report identity differ.'}
$validator=Join-Path $root 'scripts/Test-InteractiveExecutionEvidence.ps1'
& $validator -EvidenceDirectory $fixture -SourceCommit ('1'*40) -SourceRunId 42 -QualificationRunId 43
$record=[IO.File]::ReadAllText($output)
foreach ($case in @(
    @{Name='other-run';Text=$record.Replace('"qualificationRunId": "43"','"qualificationRunId": "44"')},
    @{Name='duplicate';Text=$record.Replace('"schemaVersion": 1','"schemaVersion": 1,"schemaVersion": 1')},
    @{Name='array-root';Text="[$record]"}
)) {
    [IO.File]::WriteAllText($output,$case.Text)
    $message=$null
    try {& $validator -EvidenceDirectory $fixture -SourceCommit ('1'*40) -SourceRunId 42 -QualificationRunId 43 | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw "Execution publication accepted $($case.Name)."}
    Write-Output "PASS execution publication refusal: $($case.Name)"
}
[IO.File]::WriteAllText($output,$record)
$cases=@(
    @{Name='zero';Xml=$xml.Replace('tests="27"','tests="0"')},
    @{Name='failed';Xml=$xml.Replace('failures="0"','failures="1"')},
    @{Name='skipped';Xml=$xml.Replace('skipped="0"','skipped="1"')},
    @{Name='hidden-failure';Xml=$xml.Replace('status="run" />','status="run"><failure /></testcase>')},
    @{Name='no-case';Xml=$xml.Replace('<testcase name="owned passing case" status="run" />','')},
    @{Name='missing-count';Xml=$xml.Replace(' tests="27"','')},
    @{Name='old-report';Xml=$xml.Replace($timestamp,'2020-01-01T00:00:00Z')},
    @{Name='DTD';Xml='<!DOCTYPE testsuites [<!ENTITY x "owned">]>'+$xml}
)
foreach ($case in $cases) {
    [IO.File]::WriteAllText($report,$case.Xml)
    $path=Join-Path $fixture ($case.Name+'.json')
    $message=$null
    try {& $producer -JUnitPath $report -InputContextPath $context -SourceCommit ('1'*40) -SourceRunId 42 -QualificationRunId 43 -StartedAt $started -OutputPath $path | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message -or [IO.File]::Exists($path)) {throw "Unsafe report was published: $($case.Name)"}
    Write-Output "PASS interactive report refusal: $($case.Name)"
}
$message=$null
try {& $validator -EvidenceDirectory $fixture -SourceCommit ('1'*40) -SourceRunId 42 -QualificationRunId 43 | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message -or -not $message.Contains('digest',[StringComparison]::Ordinal)) {throw "Changed report was accepted or refused at the wrong boundary: $message"}
Write-Output 'PASS: native result snapshot and execution identity are bound before publication.'
