<# Check document structure and the qualified native-notice byte inventory.
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
# These independently retained SHA256 values identify the installed copyright
# assets, not SPDX license conclusions. Refresh from a newly qualified native
# package on dependency changes; never regenerate hashes from edited notices.
# One authority here serves the public policy and its owned-copy controls.
$nativeNoticeHashes = [ordered]@{
    'adobe-xmp-core.copyright.txt'='2414861ED8E6FABCD594E122DE505506B6F56913321C00F60A9B582689EB8DF6'
    'catch2.copyright.txt'='C9BFF75738922193E67FA726FA225535870D2AA1059F91452C411736284AD566'
    'exiv2.copyright.txt'='A7BA75CB966ACA374711E2AF49E5F3AEA6A4443A803440F5D93E73A5A1222F66'
    'expat.copyright.txt'='3663555784CB098313C0FFCF0CFF77286933ECC434828E09ECF6CB209F70E146'
    'inih.copyright.txt'='9AE3B39E83E9158E44B67733BAA3BB2D84E80EFDCFB14A5820210A42C7ABDF7F'
    'libjpeg-turbo.copyright.txt'='BA6BCEEBCBA0FDD35488477C2CCA8C4632CE82C74DBFBC87D886CE6FC4433579'
    'libjpeg-turbo.README.ijg.txt'='75815E3BF6484201A3C3D17A1BBF10F2E8E3237F84DF10A2357EA896DB2A81D6'
    'libspng.copyright.txt'='C23EC2C56EBD6FF8AF3AC70E72E85AC11E9183D602D461E1712CA46DD647868F'
    'zlib.copyright.txt'='E32FF4E00D9D94930537635291DA39E7E612703334BF6FDE8C7F1686FE8A45A2'
}
foreach ($entry in $nativeNoticeHashes.GetEnumerator()) {
    $relativePath = "docs/licenses/native/$($entry.Key)"
    $path = Join-Path $RepositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $failures.Add("Missing required native notice: $relativePath")
        continue
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $entry.Value) {
        $failures.Add("Native notice bytes differ: $relativePath")
    }
}
if ($failures.Count -ne 0) { throw ($failures -join "`n") }
Write-Output 'PASS: required documentation, sections and nine retained native notice assets; links, privacy behavior and legal completeness need their own evidence.'
