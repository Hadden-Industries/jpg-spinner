<# Exercise the actual archival transition with synthetic provider responses at
   its private native-command boundary. This is local policy/ordering evidence;
   it does not claim GitHub authentication, upload or hosted qualification. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$modulePath = Join-Path $root 'scripts/ReleaseCandidateArchive.psm1'
$module = Import-Module $modulePath -Force -PassThru
$working = Join-Path $OwnedOutputRoot ('candidate-policy-' + [Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($working)
try {
    & $module {
        param($WorkingRoot)
        # Replace only the external I/O boundary. The production JSON admission,
        # metadata decisions, byte hashing, sequencing and retirement remain real.
        function script:Invoke-ReleaseCandidateGitHub {
            param($Executable, [string[]]$Arguments, $WorkingDirectory, $LogDirectory, [string]$LogName, [string]$BinaryStandardOutputPath)
            if (-not [IO.File]::Exists($Executable) -or -not [IO.Directory]::Exists($WorkingDirectory) -or -not [IO.Directory]::Exists($LogDirectory)) {
                throw 'The native command boundary requires an actual executable and owned working/log directories.'
            }
            $script:archiveCalls.Add([pscustomobject]@{name = $LogName; arguments = $Arguments })
            if ($LogName -cin @('draft-list-before', 'draft-readback', 'draft-list-after') -and
                ($Arguments -cnotcontains 'repos/Hadden-Industries/jpg-spinner/releases?per_page=100' -or
                $Arguments -cnotcontains '--paginate' -or $Arguments -cnotcontains '--slurp')) {
                throw 'Draft discovery must use the authenticated native paginated release list.'
            }
            switch ($LogName) {
                'artifact-list' {
                    $text = ConvertTo-Json -InputObject @(@{artifacts = @($script:archiveFixture.artifact) }) -Depth 12 -Compress
                    if ($script:archiveScenario -ceq 'duplicate-member') { $text = $text.Replace('"expired":false', '"expired":false,"expired":false') }
                    if ($script:archiveScenario -ceq 'duplicate-artifact') {
                        $text = ConvertTo-Json -InputObject @(@{artifacts = @($script:archiveFixture.artifact) }, @{artifacts = @($script:archiveFixture.artifact) }) -Depth 12 -Compress
                    }
                    return $text
                }
                'tag-preflight' { return $(if ($script:archiveScenario -ceq 'existing-tag') { '[{"ref":"refs/tags/existing"}]' } else { '[]' }) }
                'draft-list-before' { return '[' + (ConvertTo-Json -InputObject $script:archiveFixture.oldReleases -Depth 12 -Compress) + ']' }
                'archive-download' { [IO.File]::WriteAllBytes($BinaryStandardOutputPath, $script:archiveFixture.bytes); return }
                'draft-create' { return 'https://github.com/Hadden-Industries/jpg-spinner/releases/tag/candidate' }
                # The REST tag lookup promises a published release. Drafts are
                # discovered through the authenticated, paginated release list.
                'draft-readback' {
                    if ($script:archiveScenario -ceq 'missing-draft') { return '[[]]' }
                    return '[[],' + (ConvertTo-Json -InputObject @($script:archiveFixture.current) -Depth 12 -Compress) + ']'
                }
                'asset-readback' {
                    $bytes = $script:archiveFixture.bytes.Clone()
                    if ($script:archiveScenario -ceq 'changed-readback') { $bytes[0] = 77 }
                    [IO.File]::WriteAllBytes($BinaryStandardOutputPath, $bytes)
                    return
                }
                'old-draft-readback-11' {
                    $old = $script:archiveFixture.oldReleases[0].Clone()
                    if ($script:archiveScenario -ceq 'old-published') { $old.draft = $false }
                    return ConvertTo-Json -InputObject $old -Depth 12 -Compress
                }
                'draft-delete-11' {
                    if ($script:archiveScenario -ceq 'delete-failure') { throw 'Injected retirement failure.' }
                    return
                }
                'draft-list-after' {
                    if ($script:archiveScenario -ceq 'unresolved-retirement') { return '[' + (ConvertTo-Json -InputObject @($script:archiveFixture.oldReleases[0], $script:archiveFixture.current) -Depth 12 -Compress) + ']' }
                    return '[' + (ConvertTo-Json -InputObject @($script:archiveFixture.oldReleases[1], $script:archiveFixture.oldReleases[2], $script:archiveFixture.current) -Depth 12 -Compress) + ']'
                }
                default { throw "Unexpected synthetic provider operation: $LogName" }
            }
        }

        $source = 'aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'
        $tag = 'jpg-spinner-candidate-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa-1234-1'
        $bytes = [byte[]]@(0, 255, 128, 195, 40, 13, 10, 65, 0, 254)
        $digest = [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
        $script:archiveCalls = [Collections.Generic.List[object]]::new()
        $whatIfDestination = Join-Path $WorkingRoot 'what-if'
        Update-ReleaseCandidateArchive -SourceCommit $source -ProducerRunId 1234 -ArchiveAttempt 1 `
            -WorkingDirectory $whatIfDestination -GitHubExecutablePath (Get-Process -Id $PID).Path -WhatIf
        if ($script:archiveCalls.Count -ne 0 -or (Test-Path -LiteralPath $whatIfDestination)) { throw 'WhatIf performed candidate archival I/O.' }
        Write-Output 'PASS: candidate archive WhatIf has no filesystem or provider effects.'
        $cases = @(
            @{name = 'complete'; error = $null },
            @{name = 'wrong-source'; error = 'source' },
            @{name = 'wrong-run'; error = 'source/run identity' },
            @{name = 'expired'; error = 'expired' },
            @{name = 'wrong-length'; error = 'length' },
            @{name = 'wrong-digest'; error = 'digest' },
            @{name = 'duplicate-member'; error = 'duplicate' },
            @{name = 'duplicate-artifact'; error = 'exactly one' },
            @{name = 'existing-tag'; error = 'exists' },
            @{name = 'new-published'; error = 'unpublished' },
            @{name = 'missing-draft'; error = 'one exact release-list match' },
            @{name = 'asset-digest'; error = 'digest' },
            @{name = 'changed-readback'; error = 'readback' },
            @{name = 'old-published'; error = 'unpublished' },
            @{name = 'delete-failure'; error = 'Injected retirement failure' },
            @{name = 'unresolved-retirement'; error = 'retirement readback remains unresolved' }
        )
        foreach ($case in $cases) {
            $script:archiveScenario = $case.name
            $script:archiveCalls = [Collections.Generic.List[object]]::new()
            $artifact = @{id = 101; name = 'store-submission-evidence'; expired = $false; size_in_bytes = 10; digest = "sha256:$digest"; workflow_run = @{id = 1234; head_sha = $source } }
            $current = @{id = 22; tag_name = $tag; target_commitish = $source; draft = $true; prerelease = $true; assets = @(@{id = 33; name = 'JpgSpinner.StoreCandidate.zip'; size = 10; state = 'uploaded'; digest = "sha256:$digest" }) }
            $oldReleases = @(
                @{id = 11; tag_name = 'jpg-spinner-candidate-old'; draft = $true; prerelease = $true },
                @{id = 12; tag_name = 'jpg-spinner-candidate-published'; draft = $false; prerelease = $true },
                @{id = 13; tag_name = 'unrelated-draft'; draft = $true; prerelease = $true }
            )
            switch ($case.name) {
                'wrong-source' { $artifact.workflow_run.head_sha = 'bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb' }
                'wrong-run' { $artifact.workflow_run.id = 1235 }
                'expired' { $artifact.expired = $true }
                'wrong-length' { $artifact.size_in_bytes = 11 }
                'wrong-digest' { $artifact.digest = 'sha256:' + ('0' * 64) }
                'new-published' { $current.draft = $false }
                'asset-digest' { $current.assets[0].digest = 'sha256:' + ('0' * 64) }
            }
            $script:archiveFixture = @{artifact = $artifact; current = $current; oldReleases = $oldReleases; bytes = $bytes }
            $destination = Join-Path $WorkingRoot $case.name
            $failure = $null; $receipt = $null
            try {
                $receipt = Update-ReleaseCandidateArchive -SourceCommit $source -ProducerRunId 1234 -ArchiveAttempt 1 `
                    -WorkingDirectory $destination -GitHubExecutablePath (Get-Process -Id $PID).Path
            }
            catch { $failure = $_.Exception.Message }
            if ($null -eq $case.error) {
                if ($null -ne $failure -or $receipt.currentTag -cne $tag) { throw "Valid archival transition failed: $failure" }
                $names = @($script:archiveCalls | ForEach-Object name)
                if ([Array]::IndexOf($names, 'asset-readback') -ge [Array]::IndexOf($names, 'draft-delete-11')) {
                    throw 'An older candidate was retired before the replacement byte readback.'
                }
                $deletions = @($script:archiveCalls | Where-Object name -Like 'draft-delete-*')
                if ($deletions.Count -ne 1 -or $deletions[0].name -cne 'draft-delete-11' -or
                    $deletions[0].arguments -contains '--cleanup-tag') { throw 'Published/unrelated release or source tag entered retirement.' }
            }
            else {
                if ($null -eq $failure -or -not $failure.Contains($case.error, [StringComparison]::OrdinalIgnoreCase)) {
                    throw "Archive refusal failed at the wrong boundary: $($case.name): $failure"
                }
                $deletions = @($script:archiveCalls | Where-Object name -Like 'draft-delete-*')
                if ($case.name -cnotin @('delete-failure', 'unresolved-retirement') -and $deletions.Count -ne 0) { throw 'Failed replacement retired its predecessor.' }
                if ([IO.File]::Exists((Join-Path $destination 'archive-receipt.json'))) { throw 'Failed transition published a success receipt.' }
            }
            Write-Output "PASS: candidate archive case $($case.name)"
        }
    } $working
}
finally {
    # Restore the native implementation in callers that execute tests in-process.
    Import-Module $modulePath -Force
}
