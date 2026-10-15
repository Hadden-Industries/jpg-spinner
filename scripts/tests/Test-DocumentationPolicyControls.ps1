<# Exercise the public documentation contract on an owned realistic copy.
   These are structural refusal controls, not legal or runtime privacy proof. #>
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
$policy = Join-Path $root 'scripts/Test-DocumentationPolicy.ps1'
& $policy -RepositoryRoot $fixture
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
Write-Output 'PASS: actual documentation copy, absent-document refusal and absent-section refusal.'
