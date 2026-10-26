<# Exercise Catch2's real configuration boundary. Inherited Bazel variables must
   not truncate an arbitrary file; explicit native reporter arguments still work.
   The caller owns the evidence directory and its preservation/cleanup lifecycle. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$TestExecutable,
    [Parameter(Mandatory)][string]$EvidenceDirectory
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
Import-Module (Join-Path $repositoryRoot 'scripts/ArtifactToolProcess.psm1')
$executable = (Get-Item -LiteralPath $TestExecutable -ErrorAction Stop).FullName
$evidence = (Get-Item -LiteralPath $EvidenceDirectory -ErrorAction Stop).FullName
$caseIdentifier = [guid]::NewGuid().ToString('N')
$marker = Join-Path $evidence "$caseIdentifier-bazel-marker.txt"
$report = Join-Path $evidence "$caseIdentifier-explicit-report.xml"
$expected = [Text.Encoding]::UTF8.GetBytes('This pre-existing file must not be truncated by inherited test configuration.')
$stream = [IO.FileStream]::new($marker, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
try { $stream.Write($expected) } finally { $stream.Dispose() }
[void](Invoke-ArtifactToolProcess -Executable $executable -WorkingDirectory (Split-Path -Parent $executable) `
        -LogDirectory $evidence -LogName "$caseIdentifier-catch2-environment" `
        -Arguments @('--list-tests', '--reporter', "xml::out=$report") `
        -EnvironmentOverrides @{
        BAZEL_TEST = '1'; TEST_SHARD_INDEX = '0'; TEST_TOTAL_SHARDS = '1'; TEST_SHARD_STATUS_FILE = $marker
    })
if (-not [Linq.Enumerable]::SequenceEqual[byte]([IO.File]::ReadAllBytes($marker), $expected)) {
    throw 'Catch2 modified an environment-selected file before any test ran.'
}
# The native XML reporter is an independent positive control: disabling ambient
# configuration must not disable explicit output or test discovery.
$listing = [Xml.XmlDocument]::new()
$listing.Load($report)
if ($listing.SelectNodes('//TestCase').Count -eq 0) { throw 'The explicit reporter did not enumerate any tests.' }
Write-Output 'PASS: inherited Bazel configuration cannot write files; explicit XML reporting and discovery remain usable.'
