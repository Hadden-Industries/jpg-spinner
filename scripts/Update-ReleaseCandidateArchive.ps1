<# Archive the current protected Store workflow's attested original ZIP. This
   entry point binds provider-owned execution identity before any remote write;
   the module owns preservation, readback and replacement ordering. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$WorkingDirectory)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'ReleaseCandidateArchive.psm1')
$policy = Get-ReleaseCandidateArchivePolicy
if ($env:GITHUB_ACTIONS -cne 'true' -or $env:GITHUB_REPOSITORY -cne $policy.repository -or
    $env:GITHUB_REF -cne 'refs/heads/main') {
    throw 'Candidate archival requires this repository trusted-main Actions context.'
}
if ($env:GITHUB_SHA -cnotmatch '^[a-f0-9]{40}$') { throw 'Candidate source commit identity is invalid.' }
$runId = 0L
$attempt = 0
if (-not [long]::TryParse($env:GITHUB_RUN_ID, [Globalization.NumberStyles]::None, [Globalization.CultureInfo]::InvariantCulture, [ref]$runId) -or $runId -le 0) {
    throw 'Candidate producer run identity is invalid.'
}
if (-not [int]::TryParse($env:GITHUB_RUN_ATTEMPT, [Globalization.NumberStyles]::None, [Globalization.CultureInfo]::InvariantCulture, [ref]$attempt) -or $attempt -le 0) {
    throw 'Candidate archival attempt identity is invalid.'
}
$github = (Get-Command gh -CommandType Application -ErrorAction Stop | Select-Object -First 1).Source
$receipt = Update-ReleaseCandidateArchive -SourceCommit $env:GITHUB_SHA -ProducerRunId $runId -ArchiveAttempt $attempt `
    -WorkingDirectory $WorkingDirectory -GitHubExecutablePath $github
$summary = @(
    "Verified current unpublished candidate: [$($receipt.currentTag)](https://github.com/$($policy.repository)/releases)",
    "Source: ``$($receipt.sourceCommit)``; producer: [$runId](https://github.com/$($policy.repository)/actions/runs/$runId).",
    "Original ZIP: $($receipt.archiveByteLength) bytes, ``$($receipt.archiveDigest)``.",
    "Superseded owned candidate archives retired: $($receipt.retiredTags.Count).",
    'Archival preserves the candidate for later review; all outstanding release gates remain open.'
) -join "`n`n"
if (-not [string]::IsNullOrEmpty($env:GITHUB_STEP_SUMMARY)) {
    [IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY, $summary + "`n", [Text.UTF8Encoding]::new($false))
}
Write-Output $summary
