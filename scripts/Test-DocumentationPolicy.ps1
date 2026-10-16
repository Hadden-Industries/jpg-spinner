<# Check document structure and the retained dependency-notice byte inventory.
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
    'docs/corresponding-source.md'=@('## Scope and authority','## Release binding','## Source closure','## Reconstruction and delivery')
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
# These independently retained SHA256 values identify selected package notice
# assets, not SPDX license conclusions. Refresh from newly qualified original
# packages on dependency changes; never regenerate hashes from edited notices.
# One authority here serves the public policy and its owned-copy controls.
$retainedNoticeHashes = [ordered]@{
    'docs/licenses/native/adobe-xmp-core.copyright.txt'='2414861ED8E6FABCD594E122DE505506B6F56913321C00F60A9B582689EB8DF6'
    'docs/licenses/native/catch2.copyright.txt'='C9BFF75738922193E67FA726FA225535870D2AA1059F91452C411736284AD566'
    'docs/licenses/native/exiv2.copyright.txt'='A7BA75CB966ACA374711E2AF49E5F3AEA6A4443A803440F5D93E73A5A1222F66'
    'docs/licenses/native/expat.copyright.txt'='3663555784CB098313C0FFCF0CFF77286933ECC434828E09ECF6CB209F70E146'
    'docs/licenses/native/inih.copyright.txt'='9AE3B39E83E9158E44B67733BAA3BB2D84E80EFDCFB14A5820210A42C7ABDF7F'
    'docs/licenses/native/libjpeg-turbo.copyright.txt'='BA6BCEEBCBA0FDD35488477C2CCA8C4632CE82C74DBFBC87D886CE6FC4433579'
    'docs/licenses/native/libspng.copyright.txt'='C23EC2C56EBD6FF8AF3AC70E72E85AC11E9183D602D461E1712CA46DD647868F'
    'docs/licenses/native/zlib.copyright.txt'='E32FF4E00D9D94930537635291DA39E7E612703334BF6FDE8C7F1686FE8A45A2'
    'docs/licenses/native/libjpeg-turbo.README.ijg.txt'='75815E3BF6484201A3C3D17A1BBF10F2E8E3237F84DF10A2357EA896DB2A81D6'
    'docs/licenses/nuget/Microsoft.Web.WebView2.LICENSE.txt'='0AF8F1B807512AAE39C2AC1AA4D0CAE65CABECB6FD554B8439A5162A0D6ECA55'
    'docs/licenses/nuget/Microsoft.Web.WebView2.NOTICE.txt'='106423785C5B7EBA0A8E61D1837F2132E9C828E20AD530F565D981C1DF60DD90'
    'docs/licenses/nuget/Microsoft.Windows.AI.MachineLearning.LICENSE.txt'='66395F8CB219087FAE2BD025010BD9076B736C14F03B48F20295471C0C376814'
    'docs/licenses/nuget/Microsoft.Windows.AI.MachineLearning.ThirdPartyNotices.txt'='FB0AF774B4D7CFFC5B9D046F2AAEADE2F37DF2F80ABF8033C95DFFFCC77A8866'
    'docs/licenses/nuget/Microsoft.Windows.CppWinRT.LICENSE.txt'='C2CFCCB812FE482101A8F04597DFC5A9991A6B2748266C47AC91B6A5AAE15383'
    'docs/licenses/Microsoft.Windows.ImplementationLibrary.LICENSE.txt'='27EBDA9D51F0A56B7E281CCD8230A27236DCB51C05F64B07869ECF6E965D68B0'
    'docs/licenses/nuget/Microsoft.Windows.ImplementationLibrary.ThirdPartyNotices.txt'='32403680D9C8B4A4C68C0BF650090DCFC063F99AD8A37D7FF84E00D9709A518F'
    'docs/licenses/nuget/Microsoft.Windows.SDK.BuildTools.MSIX.NOTICE.txt'='F09EBF5DE6A5C8E7EBC900BB2E52AF2F75E13610A1CF91E1514530E448D956C3'
    'docs/licenses/nuget/Microsoft.Windows.SDK.BuildTools.MSIX.LICENSE.txt'='A7A5C7E7FF998558983D6ACA2702117C328AEB0C6404D298CB275F5623C6FD13'
    'docs/licenses/nuget/Microsoft.WindowsAppSDK.LICENSE.txt'='5B11E6347756E40FE0274BC08C97F89201B94F0D50181A09A00F1F4740840501'
    'docs/licenses/nuget/Microsoft.WindowsAppSDK.NOTICE.txt'='572B43D41DEA717DAE7DC5DE69ACB20A74DF025E8F5A3C0AA6F7BCA02615E23C'
    'docs/licenses/nuget/Microsoft.WindowsAppSDK.ML.LICENSE.txt'='656AAB74C15AA9F9964BCDCC993EB2755CBDB4822D5E0E3BC61D2E281897F758'
    'docs/licenses/nuget/Microsoft.WindowsAppSDK.WinUI.NOTICE.txt'='E25393C0D340A1821827B093FA4DBBFCCCD8FEB7BF769E7FA773E3955CD5314B'
    'docs/licenses/nuget/Microsoft.WindowsAppSDK.WinUI.Tools.NOTICE.txt'='4A9321FD173EBF4E8BBC79130794D4C3FE233ACCCBF7DB8765E0ADDB8FBFB8CA'
}
foreach ($entry in $retainedNoticeHashes.GetEnumerator()) {
    $relativePath = $entry.Key
    $path = Join-Path $RepositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        $failures.Add("Missing required dependency notice: $relativePath")
        continue
    }
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -cne $entry.Value) {
        $failures.Add("Dependency notice bytes differ: $relativePath")
    }
}
if ($failures.Count -ne 0) { throw ($failures -join "`n") }
Write-Output 'PASS: required documentation, sections and 23 retained dependency notice assets; links, privacy behavior and legal completeness need their own evidence.'
