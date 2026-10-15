<# Serialize bounded automated-UI evidence from the native Catch2 JUnit report.
   The caller must have run the fixed selection in a fresh qualified workspace
   and observed its native exit code. This serializer cannot establish desktop
   execution from an arbitrary caller-supplied XML file. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$JUnitPath,
    [Parameter(Mandatory)][string]$InputContextPath,
    [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
    [Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,18}$')][string]$SourceRunId,
    [Parameter(Mandatory)][ValidatePattern('^[1-9][0-9]{0,18}$')][string]$QualificationRunId,
    [Parameter(Mandatory)][DateTimeOffset]$StartedAt,
    [Parameter(Mandatory)][string]$OutputPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$output=[IO.Path]::GetFullPath($OutputPath)
if (Test-Path -LiteralPath $output) {throw 'Interactive evidence output must be fresh.'}
if (-not [IO.Directory]::Exists([IO.Path]::GetDirectoryName($output))) {throw 'Interactive evidence output parent must exist.'}
$completed=[DateTimeOffset]::UtcNow
if ($StartedAt -gt $completed -or ($completed-$StartedAt).TotalMinutes -gt 15) {throw 'Interactive evidence requires a current bounded run interval.'}
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1') -Force
$contextStream=$null;$reportStream=$null
try {
    # Hold both inputs read-only through publication. The report parsed and the
    # bytes hashed must be the same snapshot, not two unrelated file reads.
    $contextStream=[IO.File]::Open([IO.Path]::GetFullPath($InputContextPath),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    $reportStream=[IO.File]::Open([IO.Path]::GetFullPath($JUnitPath),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
    if ($contextStream.Length -gt 16384 -or $reportStream.Length -gt 4194304) {throw 'Interactive evidence input exceeds its size bound.'}
    $reader=[IO.StreamReader]::new($contextStream,[Text.UTF8Encoding]::new($false,$true),$true,4096,$true)
    try {$contextJson=$reader.ReadToEnd()} finally {$reader.Dispose()}
    $context=ConvertFrom-JsonWithUniqueObjectMembers -JsonText $contextJson -SourceDescription 'Authenticated interactive context' -RequiredRootValueKind Object -AsHashtable
    if (-not (Test-Json -Json $contextJson -SchemaFile (Join-Path $root 'eng/interactive-test-drop.schema.json')) -or
        $context.sourceCommit -cne $SourceCommit) {throw 'Interactive evidence context differs from the selected candidate.'}
    $settings=[Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit
    $settings.XmlResolver=$null
    $settings.MaxCharactersInDocument=4194304
    $settings.CloseInput=$false
    $reader=[Xml.XmlReader]::Create($reportStream,$settings)
    $document=[Xml.XmlDocument]::new()
    $document.XmlResolver=$null
    try {$document.Load($reader)} finally {$reader.Dispose()}
    $suites=$document.SelectNodes('/testsuites/testsuite')
    if ($document.DocumentElement.Name -cne 'testsuites' -or $suites.Count -ne 1 -or
        $suites[0].GetAttribute('name') -cne 'JpgSpinner.Presentation.Tests') {throw 'Interactive evidence requires one native presentation JUnit suite.'}
    $suite=$suites[0]
    $counts=@{}
    foreach ($name in @('tests','errors','failures','skipped')) {
        $value=[long]0
        if (-not [long]::TryParse($suite.GetAttribute($name),[Globalization.NumberStyles]::None,[Globalization.CultureInfo]::InvariantCulture,[ref]$value)) {
            throw 'Interactive JUnit suite omits a nonnegative native count.'
        }
        $counts[$name]=$value
    }
    $cases=$suite.SelectNodes('testcase')
    if ($counts.tests -lt 1 -or $counts.errors -ne 0 -or $counts.failures -ne 0 -or $counts.skipped -ne 0 -or
        $cases.Count -lt 1 -or $document.SelectNodes('//failure|//error|//skipped|//internalError').Count -ne 0) {
        throw 'Interactive evidence requires actual nonempty execution without failures, errors or skips.'
    }
    $timestamp=[DateTimeOffset]::MinValue
    if (-not [DateTimeOffset]::TryParseExact($suite.GetAttribute('timestamp'),'yyyy-MM-ddTHH:mm:ssZ',
        [Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::AssumeUniversal,[ref]$timestamp) -or
        $timestamp -lt $StartedAt.AddSeconds(-1) -or $timestamp -gt $completed) {throw 'Interactive JUnit timestamp is outside the fresh run interval.'}
    $contextStream.Position=0;$reportStream.Position=0
    $evidence=@{
        schemaVersion=1;scope='automated-ui-only';sourceCommit=$SourceCommit
        sourceRunId=$SourceRunId;qualificationRunId=$QualificationRunId
        inputContextSha256=[Convert]::ToHexStringLower([Security.Cryptography.SHA256]::HashData($contextStream))
        junitSha256=[Convert]::ToHexStringLower([Security.Cryptography.SHA256]::HashData($reportStream))
        startedAt=$StartedAt.ToUniversalTime().ToString('O');completedAt=$completed.ToString('O')
        assertionCount=$counts.tests;reportedCaseCount=$cases.Count
        requestedTestSpecification='[ui-automation]~[operator-assisted]'
        operatorAcceptance='pending'
    }
    $json=$evidence | ConvertTo-Json
    if (-not (Test-Json -Json $json -SchemaFile (Join-Path $root 'eng/interactive-test-evidence.schema.json'))) {throw 'Interactive evidence violates its output contract.'}
    $stream=[IO.File]::Open($output,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None)
    try {
        $bytes=[Text.UTF8Encoding]::new($false).GetBytes($json+"`n")
        $stream.Write($bytes)
    } finally {$stream.Dispose()}
} finally {
    if ($null -ne $reportStream) {$reportStream.Dispose()}
    if ($null -ne $contextStream) {$contextStream.Dispose()}
}
Write-Output $output
