<# Check the maintained documentation's declared structural contract.
   This is not a Markdown parser, link checker, legal opinion or assertion that
   every sentence matches runtime behavior. Release sign-offs remain separate. #>
[CmdletBinding()]
param([string]$RepositoryRoot=(Split-Path -Parent $PSScriptRoot))
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$requirements = [ordered]@{
    'README.md'=@('## Status','## Safe defaults','## Supported inputs','## Build and test','## License')
    'SECURITY.md'=@('## Supported versions','## Private reporting','## Threat model','## Disclosure')
    'docs/architecture.md'=@('## Modules','## Transaction invariants','## Security boundary','## Guarantee limits')
    'docs/privacy.md'=@('## Local processing','## Diagnostic records','## Retention','## Export')
    'docs/release-qualification.md'=@('## Gate register','## Evidence identity','## Release authorization')
    'THIRD_PARTY_NOTICES.md'=@('## Qualification status','## Native components','## Windows packages','## Engineering tools')
}
$failures = [Collections.Generic.List[string]]::new()
foreach ($entry in $requirements.GetEnumerator()) {
    $path = Join-Path $RepositoryRoot $entry.Key
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { $failures.Add("Missing required document: $($entry.Key)"); continue }
    $text = Get-Content -LiteralPath $path -Raw
    foreach ($heading in $entry.Value) {
        # Literal line identity is the project's small content contract, not a
        # substitute implementation of Markdown grammar or semantic validation.
        if ($heading -cnotin ($text -split '\r?\n')) { $failures.Add("Missing required section $heading in $($entry.Key)") }
    }
}
if ($failures.Count -ne 0) { throw ($failures -join "`n") }
Write-Output 'PASS: required documentation and section contract; links, privacy behavior and legal completeness need their own evidence.'
