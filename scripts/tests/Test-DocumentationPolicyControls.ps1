<# Exercise document structure and retained native-notice integrity on an owned
   copy. Hash controls detect lost/edited notices, not legal completeness. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$fixture = Join-Path $OwnedOutputRoot ('documentation-contract-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory((Join-Path $fixture 'docs'))
foreach ($path in @('README.md','SECURITY.md','THIRD_PARTY_NOTICES.md','docs/architecture.md','docs/privacy.md','docs/release-qualification.md')) {
    Copy-Item -LiteralPath (Join-Path $root $path) -Destination (Join-Path $fixture $path)
}
$nativeNotices = Join-Path $root 'docs/licenses/native'
if (Test-Path -LiteralPath $nativeNotices -PathType Container) {
    [void][IO.Directory]::CreateDirectory((Join-Path $fixture 'docs/licenses'))
    Copy-Item -LiteralPath $nativeNotices -Destination (Join-Path $fixture 'docs/licenses/native') -Recurse
}
$policy = Join-Path $root 'scripts/Test-DocumentationPolicy.ps1'
& $policy -RepositoryRoot $fixture
# The native package's actual notice, rather than a shortened SPDX label, must
# remain available. A missing-file fixture also detects an ineffective policy.
$jpegNotice = Join-Path $fixture 'docs/licenses/native/libjpeg-turbo.copyright.txt'
$heldJpegNotice = "$jpegNotice.held"
if (Test-Path -LiteralPath $jpegNotice -PathType Leaf) {
    Move-Item -LiteralPath $jpegNotice -Destination $heldJpegNotice
}
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Missing required native notice: docs/licenses/native/libjpeg-turbo.copyright.txt',[StringComparison]::Ordinal)) {
    throw 'Missing-native-notice control did not reach its precise refusal.'
}
Move-Item -LiteralPath $heldJpegNotice -Destination $jpegNotice
$jpegNoticeBytes = [IO.File]::ReadAllBytes($jpegNotice)
# Removing notice text is a realistic regression even when the document and
# all headings still exist. Restore exact bytes so later controls stay isolated.
[IO.File]::WriteAllText($jpegNotice,'Short SPDX label is not the packaged notice.')
$message=$null
try { & $policy -RepositoryRoot $fixture } catch { $message=$_.Exception.Message }
if ($null -eq $message -or -not $message.Contains('Native notice bytes differ: docs/licenses/native/libjpeg-turbo.copyright.txt',[StringComparison]::Ordinal)) {
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
Write-Output 'PASS: documentation copy; missing/edited native notice, missing document and missing section refusals.'
