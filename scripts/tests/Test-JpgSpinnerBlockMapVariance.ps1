<# Native writers supply policy fixtures, not purported validated containers.
   Original-package validation belongs to the outer bundle-comparison route. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$OwnedOutputRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$test=Join-Path $root 'scripts/Test-JpgSpinnerBlockMapVariance.ps1'
$fixture=Join-Path $OwnedOutputRoot ('block-map-'+[Guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixture)
Import-Module (Join-Path $PSScriptRoot 'Fixtures/WinmdFixture.psm1') -Force
$firstWinmd=New-TestWinmd $fixture first ([Guid]'384e147e-1a1d-46c4-837e-25af7c907767') 1
$secondWinmd=New-TestWinmd $fixture second ([Guid]'68497c1d-f61d-4c8e-b5e6-c24f8f2f4b6f') 2
$size=(Get-Item -LiteralPath $firstWinmd).Length
if ($size -le 0 -or $size -gt 65536 -or $size -ne (Get-Item -LiteralPath $secondWinmd).Length) {throw 'Native fixture must have one equal-length data block.'}
$firstHash=[Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([IO.File]::ReadAllBytes($firstWinmd)))
$secondHash=[Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([IO.File]::ReadAllBytes($secondWinmd)))
if ($firstHash -ceq $secondHash) {throw 'Native fixture must actually change its WinMD content hash.'}
function New-TestBlockMap([string]$Name,[string]$Hash,[int]$CompressedSize,[bool]$ChangeOther=$false,[int]$LocalHeaderSize=46,[bool]$AddComment=$false,[int]$UncompressedSize=$size) {
    <# XMLWriter owns syntax; literal expected policy decisions stay below. #>
    $path=Join-Path $fixture ($Name+'.xml')
    $settings=[Xml.XmlWriterSettings]::new();$settings.Encoding=[Text.UTF8Encoding]::new($false)
    $writer=[Xml.XmlWriter]::Create($path,$settings)
    try {
        $writer.WriteStartDocument($false)
        $writer.WriteStartElement('BlockMap','http://schemas.microsoft.com/appx/2010/blockmap')
        $writer.WriteAttributeString('HashMethod','http://www.w3.org/2001/04/xmlenc#sha256')
        $writer.WriteStartElement('File');$writer.WriteAttributeString('Name','JpgSpinner.winmd')
        $writer.WriteAttributeString('Size',$UncompressedSize.ToString([Globalization.CultureInfo]::InvariantCulture))
        $writer.WriteAttributeString('LfhSize',$LocalHeaderSize.ToString([Globalization.CultureInfo]::InvariantCulture))
        $writer.WriteStartElement('Block');$writer.WriteAttributeString('Hash',$Hash)
        $writer.WriteAttributeString('Size',$CompressedSize.ToString([Globalization.CultureInfo]::InvariantCulture))
        $writer.WriteEndElement();$writer.WriteEndElement()
        $writer.WriteStartElement('File');$writer.WriteAttributeString('Name','other.bin');$writer.WriteAttributeString('Size','1');$writer.WriteAttributeString('LfhSize','39')
        $writer.WriteStartElement('Block')
        $writer.WriteAttributeString('Hash',[Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([byte[]]@([byte]$ChangeOther))))
        $writer.WriteEndElement();$writer.WriteEndElement()
        if($AddComment){$writer.WriteComment('unapproved unrelated XML change')}
        $writer.WriteEndElement();$writer.WriteEndDocument()
    } finally {$writer.Dispose()}
    return $path
}
$firstMap=New-TestBlockMap first $firstHash 300
$secondMap=New-TestBlockMap second $secondHash 301
$before=@{}
foreach ($path in @($firstWinmd,$secondWinmd,$firstMap,$secondMap)) {$before[$path]=(Get-FileHash -LiteralPath $path).Hash}
$same=& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $firstMap -FirstWinmdPath $firstWinmd -SecondWinmdPath $firstWinmd
if (-not $same.ByteIdentical) {throw 'Identical block maps were not accepted truthfully.'}
$different=& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $secondMap -FirstWinmdPath $firstWinmd -SecondWinmdPath $secondWinmd
if ($different.ByteIdentical -or $different.ChangedBlocks.Count -ne 1) {throw 'Qualified hash/compressed-size differences were concealed or not accepted.'}
if ($different.ChangedBlocks[0].FirstCompressedSizeBytes -ne 300 -or $different.ChangedBlocks[0].SecondCompressedSizeBytes -ne 301) {throw 'Compressed block byte lengths were not reported accurately.'}
$widerSize=New-TestBlockMap wider-size $secondHash 1000
$wider=& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $widerSize -FirstWinmdPath $firstWinmd -SecondWinmdPath $secondWinmd
if ($wider.ByteIdentical -or $wider.ChangedBlocks[0].SecondCompressedSizeBytes -ne 1000) {throw 'Allowed size digit-count change corrupted the untouched-byte comparison.'}
$sameSizeMap=New-TestBlockMap hash-only $secondHash 300
$sameSize=& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $sameSizeMap -FirstWinmdPath $firstWinmd -SecondWinmdPath $secondWinmd
if ($sameSize.ByteIdentical -or $sameSize.ChangedBlocks[0].SecondCompressedSizeBytes -ne 300) {throw 'A changed hash with unchanged compressed length was not accepted truthfully.'}
$cases=@(
    @{Name='unchanged-hash-size';Map=(New-TestBlockMap unchanged-hash $firstHash 301)},
    @{Name='unrelated-file';Map=(New-TestBlockMap other-file $secondHash 301 $true)},
    @{Name='local-header';Map=(New-TestBlockMap header $secondHash 301 $false 47)},
    @{Name='unrelated-xml';Map=(New-TestBlockMap comment $secondHash 301 $false 46 $true)},
    @{Name='uncompressed-file-size';Map=(New-TestBlockMap file-size $secondHash 301 $false 46 $false ($size+1))}
)
foreach ($case in $cases) {
    $message=$null
    try {& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $case.Map -FirstWinmdPath $firstWinmd -SecondWinmdPath $secondWinmd | Out-Null}
    catch {$message=$_.Exception.Message}
    if ($null -eq $message) {throw "Unapproved block-map variance passed: $($case.Name)"}
}
$message=$null
try {& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $secondMap -FirstWinmdPath $firstWinmd -SecondWinmdPath $firstWinmd | Out-Null}
catch {$message=$_.Exception.Message}
if ($null -eq $message) {throw 'Changed block hash passed without an actual WinMD identity change.'}
foreach ($path in $before.Keys) {if ((Get-FileHash -LiteralPath $path).Hash -cne $before[$path]) {throw 'Comparison changed an original fixture.'}}
Write-Output 'PASS: qualified WinMD block hashes/sizes accepted; unchanged-hash sizes, unrelated files/XML and local-header differences refused.'
