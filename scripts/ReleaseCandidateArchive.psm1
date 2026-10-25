<# Native GitHub release archival for one current unpublished candidate. The
   provider CLI owns HTTP, redirects, pagination and command-line semantics;
   the existing platform JSON boundary owns syntax and duplicate admission. #>
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'ArtifactToolProcess.psm1')
Import-Module (Join-Path $PSScriptRoot 'QualificationPolicy.psm1')
Import-Module (Join-Path $PSScriptRoot 'JsonObjectMemberValidation.psm1')

function Get-ReleaseCandidateArchivePolicy {
    <# Return the single authority for the managed namespace and transport name. #>
    [CmdletBinding()]
    param()
    return [pscustomobject]@{
        repository         ='Hadden-Industries/jpg-spinner'
        tagPrefix          ='jpg-spinner-candidate-'
        sourceArtifactName ='store-submission-evidence'
        archiveAssetName   ='JpgSpinner.StoreCandidate.zip'
    }
}

function Read-CandidateProviderJson {
    param([string]$Text, [Text.Json.JsonValueKind]$RootKind, [string]$Description)
    [void](ConvertFrom-JsonWithUniqueObjectMembers -JsonText $Text -RequiredRootValueKind $RootKind -SourceDescription $Description)
    $document = [Text.Json.JsonDocument]::Parse($Text)
    try { return $document.RootElement.Clone() } finally { $document.Dispose() }
}

function Read-CandidatePositiveInteger {
    param([Text.Json.JsonElement]$Value)
    # JSON Schema integer values may use integral decimal/exponent forms.
    # GetDecimal preserves that meaning before checked native Int64 narrowing.
    $number = $Value.GetDecimal()
    if ($number -le 0 -or $number -gt [long]::MaxValue -or [decimal]::Truncate($number) -ne $number) {
        throw 'Candidate provider integer is outside its positive Int64 contract.'
    }
    return [long]$number
}

function Get-CandidateDraft {
    param([Text.Json.JsonElement]$Pages, $Policy)
    foreach ($page in $Pages.EnumerateArray()) {
        foreach ($release in $page.EnumerateArray()) {
            $tag = $release.GetProperty('tag_name').GetString()
            if ($null -eq $tag) { throw 'Release tag identity is absent.' }
            if ($tag.StartsWith($Policy.tagPrefix, [StringComparison]::Ordinal) -and
                $release.GetProperty('draft').GetBoolean() -and $release.GetProperty('prerelease').GetBoolean()) {
                [void](Read-CandidatePositiveInteger $release.GetProperty('id'))
                $release
            }
        }
    }
}

function Assert-CandidateDraft {
    param([Text.Json.JsonElement]$Release, [string]$Tag, [long]$ReleaseId)
    if (-not $Release.GetProperty('draft').GetBoolean() -or -not $Release.GetProperty('prerelease').GetBoolean()) {
        throw 'Candidate archive must remain an unpublished prerelease draft.'
    }
    if ($Release.GetProperty('tag_name').GetString() -cne $Tag -or
        (Read-CandidatePositiveInteger $Release.GetProperty('id')) -ne $ReleaseId) {
        throw 'Candidate draft identity changed during replacement.'
    }
}

function Invoke-ReleaseCandidateGitHub {
    param([string]$Executable, [string[]]$Arguments, [string]$WorkingDirectory,
        [string]$LogDirectory, [string]$LogName, [string]$BinaryStandardOutputPath)
    $parameters = @{Executable = $Executable; Arguments = $Arguments; WorkingDirectory = $WorkingDirectory; LogDirectory = $LogDirectory; LogName = $LogName }
    if (-not [string]::IsNullOrEmpty($BinaryStandardOutputPath)) {
        $parameters.BinaryStandardOutputPath = $BinaryStandardOutputPath
        $parameters.TimeoutSeconds = Get-QualificationTimeout -Category artifactDownload
    }
    return Invoke-ArtifactToolProcess @parameters
}

function Update-ReleaseCandidateArchive {
    <# Preserve one exact provider ZIP, verify its uploaded bytes, then retire
       only the previously observed owned unpublished drafts. Unknown/failing
       operations throw without a success receipt or blind retry. GitHub's
       workflow concurrency coordinates writers; this is not an atomic API
       transaction or protection against an uncoordinated administrator. #>
    [CmdletBinding(SupportsShouldProcess)]
    param(
        [Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40}$')][string]$SourceCommit,
        [Parameter(Mandatory)][ValidateScript({ $_ -gt 0 })][long]$ProducerRunId,
        [Parameter(Mandatory)][ValidateScript({ $_ -gt 0 })][int]$ArchiveAttempt,
        [Parameter(Mandatory)][string]$WorkingDirectory,
        [Parameter(Mandatory)][string]$GitHubExecutablePath
    )
    $policy = Get-ReleaseCandidateArchivePolicy
    $repository = $policy.repository
    $tag = "$($policy.tagPrefix)$SourceCommit-$ProducerRunId-$ArchiveAttempt"
    # One decision covers the ordered transition. WhatIf must not allocate a
    # workspace, contact GitHub or retire anything before returning.
    if (-not $PSCmdlet.ShouldProcess("$repository candidate $tag", 'Archive, verify uploaded bytes and retire superseded owned unpublished drafts')) { return }
    if ([IO.Directory]::Exists($WorkingDirectory) -or [IO.File]::Exists($WorkingDirectory)) {
        throw 'Existing candidate archival workspace requires outcome reconciliation.'
    }
    [void][IO.Directory]::CreateDirectory($WorkingDirectory)
    $logs = Join-Path $WorkingDirectory 'logs'
    [void][IO.Directory]::CreateDirectory($logs)
    $context = @{Executable = $GitHubExecutablePath; WorkingDirectory = $WorkingDirectory; LogDirectory = $logs }
    function Invoke-CandidateApi {
        param([string]$Endpoint, [string]$LogName, [switch]$Paginate, [string]$BinaryPath, [string]$Accept = 'application/vnd.github+json')
        $arguments = @('api', '--hostname', 'github.com', '-H', "Accept: $Accept", '-H', 'X-GitHub-Api-Version: 2026-03-10', $Endpoint)
        if ($Paginate) { $arguments += @('--paginate', '--slurp') }
        $parameters = @{Arguments = $arguments; LogName = $LogName }
        if (-not [string]::IsNullOrEmpty($BinaryPath)) { $parameters.BinaryStandardOutputPath = $BinaryPath }
        Invoke-ReleaseCandidateGitHub @context @parameters
    }
    $pages = Read-CandidateProviderJson -Text (Invoke-CandidateApi "repos/$repository/actions/runs/$ProducerRunId/artifacts?per_page=100" 'artifact-list' -Paginate) -RootKind Array -Description 'GitHub candidate artifact pages'
    $artifacts = @(foreach ($page in $pages.EnumerateArray()) {
            foreach ($item in $page.GetProperty('artifacts').EnumerateArray()) {
                if ($item.GetProperty('name').GetString() -ceq $policy.sourceArtifactName) { $item }
            }
        })
    if ($artifacts.Count -ne 1) { throw 'Candidate producer requires exactly one attested evidence artifact.' }
    $artifact = $artifacts[0]
    if ($artifact.GetProperty('expired').GetBoolean()) { throw 'Candidate evidence artifact has expired.' }
    $producer = $artifact.GetProperty('workflow_run')
    if ((Read-CandidatePositiveInteger $producer.GetProperty('id')) -ne $ProducerRunId -or
        $producer.GetProperty('head_sha').GetString() -cne $SourceCommit) { throw 'Candidate artifact source/run identity differs.' }
    $artifactId = Read-CandidatePositiveInteger $artifact.GetProperty('id')
    $length = Read-CandidatePositiveInteger $artifact.GetProperty('size_in_bytes')
    $digest = $artifact.GetProperty('digest').GetString()
    if ($null -eq $digest -or $digest -cnotmatch '^sha256:[a-f0-9]{64}$') { throw 'Candidate artifact digest is absent or unsupported.' }
    if ($length -ge 2GB) { throw 'Candidate archive exceeds the native release-asset length limit.' }
    # Native matching-refs returns an empty array when the generated ref is
    # absent. Refuse existing refs instead of trusting target_commitish, which
    # GitHub ignores when a tag already exists.
    $refs = Read-CandidateProviderJson -Text (Invoke-CandidateApi "repos/$repository/git/matching-refs/tags/$tag" 'tag-preflight') -RootKind Array -Description 'Candidate Git refs'
    if ($refs.GetArrayLength() -ne 0) { throw 'Candidate source tag already exists; reconcile rather than overwrite it.' }
    $before = Read-CandidateProviderJson -Text (Invoke-CandidateApi "repos/$repository/releases?per_page=100" 'draft-list-before' -Paginate) -RootKind Array -Description 'GitHub candidate release pages'
    $previous = @(Get-CandidateDraft -Pages $before -Policy $policy)
    if (@($previous | Where-Object { $_.GetProperty('tag_name').GetString() -ceq $tag }).Count -ne 0) {
        throw 'Candidate draft already exists; reconcile the prior outcome.'
    }
    $archive = Join-Path $WorkingDirectory $policy.archiveAssetName
    Invoke-CandidateApi "repos/$repository/actions/artifacts/$artifactId/zip" 'archive-download' -BinaryPath $archive | Out-Null
    if ((Get-Item -LiteralPath $archive).Length -ne $length) { throw 'Original candidate archive length differs.' }
    if ('sha256:' + (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -cne $digest) {
        throw 'Original candidate archive digest differs.'
    }
    $notes = Join-Path $WorkingDirectory 'candidate-notes.md'
    [IO.File]::WriteAllText($notes, (@(
                'Unpublished candidate under qualification; remaining release gates stay open.',
                "Source commit: $SourceCommit", "Producer run: https://github.com/$repository/actions/runs/$ProducerRunId",
                "Original artifact: $artifactId", "Original ZIP digest: $digest", "Archival operation attempt: $ArchiveAttempt"
            ) -join "`n") + "`n", [Text.UTF8Encoding]::new($false))
    Invoke-ReleaseCandidateGitHub @context -LogName 'draft-create' -Arguments @(
        'release', 'create', $tag, $archive, '--repo', $repository, '--draft', '--prerelease', '--target', $SourceCommit,
        '--title', "JPG Spinner release candidate $($SourceCommit.Substring(0,12))", '--notes-file', $notes
    ) | Out-Null
    # GitHub's tag endpoint promises a published release, not an unpublished
    # draft. The authenticated release list includes drafts for push-capable
    # callers; paginate it and select the exact generated tag without losing
    # unexpected published/duplicate matches before the state checks below.
    $createdPages = Read-CandidateProviderJson -Text (Invoke-CandidateApi "repos/$repository/releases?per_page=100" 'draft-readback' -Paginate) -RootKind Array -Description 'New candidate release pages'
    $created = @(foreach ($page in $createdPages.EnumerateArray()) {
            foreach ($release in $page.EnumerateArray()) {
                if ($release.GetProperty('tag_name').GetString() -ceq $tag) { $release }
            }
        })
    if ($created.Count -ne 1) { throw 'New candidate draft requires one exact release-list match.' }
    $current = $created[0]
    $releaseId = Read-CandidatePositiveInteger $current.GetProperty('id')
    Assert-CandidateDraft -Release $current -Tag $tag -ReleaseId $releaseId
    if ($current.GetProperty('target_commitish').GetString() -cne $SourceCommit) { throw 'Archived candidate source differs.' }
    $assets = $current.GetProperty('assets')
    if ($assets.GetArrayLength() -ne 1) { throw 'Candidate archive requires exactly its declared original ZIP asset.' }
    $asset = $assets[0]
    if ($asset.GetProperty('name').GetString() -cne $policy.archiveAssetName -or
        $asset.GetProperty('state').GetString() -cne 'uploaded') { throw 'Candidate archive asset is incomplete.' }
    if ((Read-CandidatePositiveInteger $asset.GetProperty('size')) -ne $length) { throw 'Uploaded candidate asset length differs.' }
    if ($asset.GetProperty('digest').GetString() -cne $digest) { throw 'Uploaded candidate asset digest differs.' }
    $assetId = Read-CandidatePositiveInteger $asset.GetProperty('id')
    $readback = Join-Path $WorkingDirectory 'candidate-readback.zip'
    Invoke-CandidateApi "repos/$repository/releases/assets/$assetId" 'asset-readback' -Accept 'application/octet-stream' -BinaryPath $readback | Out-Null
    if ((Get-Item -LiteralPath $readback).Length -ne $length -or
        'sha256:' + (Get-FileHash -LiteralPath $readback -Algorithm SHA256).Hash.ToLowerInvariant() -cne $digest) {
        throw 'Candidate archive byte readback differs; predecessor retirement is refused.'
    }
    $retired = [Collections.Generic.List[string]]::new()
    foreach ($old in $previous) {
        $oldId = Read-CandidatePositiveInteger $old.GetProperty('id')
        $oldTag = $old.GetProperty('tag_name').GetString()
        $fresh = Read-CandidateProviderJson -Text (Invoke-CandidateApi "repos/$repository/releases/$oldId" "old-draft-readback-$oldId") -RootKind Object -Description 'Superseded candidate draft'
        Assert-CandidateDraft -Release $fresh -Tag $oldTag -ReleaseId $oldId
        # This removes the owned unpublished release/assets, preserving Git tags
        # and source history. Never add --cleanup-tag or delete whole runs.
        Invoke-ReleaseCandidateGitHub @context -LogName "draft-delete-$oldId" -Arguments @('release', 'delete', $oldTag, '--repo', $repository, '--yes') | Out-Null
        $retired.Add($oldTag)
    }
    $after = Read-CandidateProviderJson -Text (Invoke-CandidateApi "repos/$repository/releases?per_page=100" 'draft-list-after' -Paginate) -RootKind Array -Description 'Candidate retirement readback'
    $remaining = @(Get-CandidateDraft -Pages $after -Policy $policy)
    if ($remaining.Count -ne 1 -or (Read-CandidatePositiveInteger $remaining[0].GetProperty('id')) -ne $releaseId) {
        throw 'Single-current-candidate retirement readback remains unresolved.'
    }
    $receipt = [pscustomobject]@{schemaVersion=1; currentReleaseId=$releaseId; currentTag=$tag; sourceCommit=$SourceCommit;
        producerRunId=$ProducerRunId; archiveAttempt=$ArchiveAttempt; originalArtifactId=$artifactId;
        archiveByteLength=$length; archiveDigest=$digest; retiredTags=@($retired); state='verified-unpublished-candidate'
    }
    [IO.File]::WriteAllText((Join-Path $WorkingDirectory 'archive-receipt.json'), ($receipt | ConvertTo-Json -Depth 5) + "`n", [Text.UTF8Encoding]::new($false))
    return $receipt
}

Export-ModuleMember -Function Get-ReleaseCandidateArchivePolicy, Update-ReleaseCandidateArchive
