<# Exercise document structure and retained dependency-notice integrity on an owned
   copy. Hash controls detect lost/edited notices, not legal completeness. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
# Use Git's actual matcher, including its directory-negation semantics. A local
# untracked draft can satisfy the document policy yet disappear from a commit.
& git -C $root check-ignore --no-index --quiet -- 'docs/releases/2.0.0.md'
$documentationIgnoreExit = $LASTEXITCODE
if ($documentationIgnoreExit -ne 1) { throw 'Release-note source must be visible to native Git selection.' }
& git -C $root check-ignore --no-index --quiet -- 'docs/releases/qualification-fixture.msixbundle'
$packageIgnoreExit = $LASTEXITCODE
if ($packageIgnoreExit -ne 0) { throw 'Generated release packages must remain ignored; no package fixture is created.' }
$fixture = Join-Path $OwnedOutputRoot ('documentation-contract-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'docs'))
foreach ($path in @('README.md','SECURITY.md','THIRD_PARTY_NOTICES.md','docs/architecture.md','docs/privacy.md','docs/release-qualification.md','docs/corresponding-source.md','docs/releases/2.0.0.md')) {
    [void][IO.Directory]::CreateDirectory((Split-Path -Parent (Join-Path $fixture $path)))
    Copy-Item -LiteralPath (Join-Path $root $path) -Destination (Join-Path $fixture $path)
}
$sourceDeliveryPath = Join-Path $fixture 'docs/corresponding-source.md'
$dependencyNotices = Join-Path $root 'docs/licenses'
if (Test-Path -LiteralPath $dependencyNotices -PathType Container) {
    Copy-Item -LiteralPath $dependencyNotices -Destination (Join-Path $fixture 'docs/licenses') -Recurse
}
$policy = Join-Path $root 'scripts/Test-DocumentationPolicy.ps1'
& $policy -RepositoryRoot $fixture
# Task 20's release notes are a maintained draft until all release gates pass.
# Exercise the real policy boundary, not just the existence of another document.
$releaseNotesPath = Join-Path $fixture 'docs/releases/2.0.0.md'
$heldReleaseNotesPath = "$releaseNotesPath.held"
Move-Item -LiteralPath $releaseNotesPath -Destination $heldReleaseNotesPath
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required document: docs/releases/2.0.0.md',[StringComparison]::Ordinal)) {
    throw 'Missing-release-notes control did not reach its precise refusal.'
}
Move-Item -LiteralPath $heldReleaseNotesPath -Destination $releaseNotesPath
$releaseNotes = Get-Content -LiteralPath $releaseNotesPath -Raw
# A release-note file without its qualification boundary must not satisfy the
# structural contract merely because the remaining feature prose is present.
[IO.File]::WriteAllText($releaseNotesPath,$releaseNotes.Replace('## Qualification status','## Unrelated section',[StringComparison]::Ordinal))
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required section ## Qualification status in docs/releases/2.0.0.md',[StringComparison]::Ordinal)) {
    throw 'Missing-release-qualification-section control did not reach its precise refusal.'
}
[IO.File]::WriteAllText($releaseNotesPath,$releaseNotes)
# An application-source URL or a binary SDK export alone cannot describe the
# dependency sources, patches and reconstruction required at distribution.
$heldSourceDeliveryPath = "$sourceDeliveryPath.held"
Move-Item -LiteralPath $sourceDeliveryPath -Destination $heldSourceDeliveryPath
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required document: docs/corresponding-source.md',[StringComparison]::Ordinal)) {
    throw 'Missing-corresponding-source-procedure control did not reach its precise refusal.'
}
Move-Item -LiteralPath $heldSourceDeliveryPath -Destination $sourceDeliveryPath
$sourceDelivery = Get-Content -LiteralPath $sourceDeliveryPath -Raw
[IO.File]::WriteAllText($sourceDeliveryPath,$sourceDelivery.Replace('## Reconstruction and delivery','## Unrelated section',[StringComparison]::Ordinal))
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required section ## Reconstruction and delivery in docs/corresponding-source.md',[StringComparison]::Ordinal)) {
    throw 'Missing-source-reconstruction-section control did not reach its precise refusal.'
}
[IO.File]::WriteAllText($sourceDeliveryPath,$sourceDelivery)
# NuGet licensing is package-specific; an existing native-port inventory must
# not silently stand in for the resolved Microsoft package notices.
$webViewNotice = Join-Path $fixture 'docs/licenses/nuget/Microsoft.Web.WebView2.LICENSE.txt'
$heldWebViewNotice = "$webViewNotice.held"
if (Test-Path -LiteralPath $webViewNotice -PathType Leaf) {
    Move-Item -LiteralPath $webViewNotice -Destination $heldWebViewNotice
}
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required dependency notice: docs/licenses/nuget/Microsoft.Web.WebView2.LICENSE.txt',[StringComparison]::Ordinal)) {
    throw 'Missing-NuGet-notice control did not reach its precise refusal.'
}
Move-Item -LiteralPath $heldWebViewNotice -Destination $webViewNotice
# A present Microsoft package notice must also remain complete; this control
# exercises the same public boundary without interpreting proprietary terms.
$webViewNoticeBytes=[IO.File]::ReadAllBytes($webViewNotice)
[IO.File]::WriteAllText($webViewNotice,'A package name is not its license text.')
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Dependency notice bytes differ: docs/licenses/nuget/Microsoft.Web.WebView2.LICENSE.txt',[StringComparison]::Ordinal)) {
    throw 'Edited-NuGet-notice control did not reach its precise refusal.'
}
[IO.File]::WriteAllBytes($webViewNotice,$webViewNoticeBytes)
# The native package's actual notice, rather than a shortened SPDX label, must
# remain available. A missing-file fixture also detects an ineffective policy.
$jpegNotice = Join-Path $fixture 'docs/licenses/native/libjpeg-turbo.copyright.txt'
$heldJpegNotice = "$jpegNotice.held"
if (Test-Path -LiteralPath $jpegNotice -PathType Leaf) {
    Move-Item -LiteralPath $jpegNotice -Destination $heldJpegNotice
}
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required dependency notice: docs/licenses/native/libjpeg-turbo.copyright.txt',[StringComparison]::Ordinal)) {
    throw 'Missing-native-notice control did not reach its precise refusal.'
}
Move-Item -LiteralPath $heldJpegNotice -Destination $jpegNotice
$jpegNoticeBytes = [IO.File]::ReadAllBytes($jpegNotice)
# Removing notice text is a realistic regression even when the document and
# all headings still exist. Restore exact bytes so later controls stay isolated.
[IO.File]::WriteAllText($jpegNotice,'Short SPDX label is not the packaged notice.')
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Dependency notice bytes differ: docs/licenses/native/libjpeg-turbo.copyright.txt',[StringComparison]::Ordinal)) {
    throw 'Edited-native-notice control did not reach its precise refusal.'
}
[IO.File]::WriteAllBytes($jpegNotice,$jpegNoticeBytes)
# Moving only this owned copy proves a real absent-document refusal without
# deleting source or altering unrelated checkout files.
Move-Item -LiteralPath (Join-Path $fixture 'SECURITY.md') -Destination (Join-Path $fixture 'SECURITY.held.md')
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required document: SECURITY.md',[StringComparison]::Ordinal)) {
    throw 'Missing-document control did not reach its precise structural refusal.'
}
Move-Item -LiteralPath (Join-Path $fixture 'SECURITY.held.md') -Destination (Join-Path $fixture 'SECURITY.md')
$privacyPath = Join-Path $fixture 'docs/privacy.md'
$privacy = Get-Content -LiteralPath $privacyPath -Raw
[IO.File]::WriteAllText($privacyPath,$privacy.Replace('## Export','## Unrelated section',[StringComparison]::Ordinal))
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required section ## Export in docs/privacy.md',[StringComparison]::Ordinal)) {
    throw 'Missing-section control did not reach its precise structural refusal.'
}
Write-Output 'PASS: documentation copy; missing release notes/qualification section, source procedure/reconstruction section, missing/edited native and NuGet notices, missing document and missing section refusals.'
