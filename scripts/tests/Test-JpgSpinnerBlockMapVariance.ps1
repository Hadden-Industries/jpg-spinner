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
$before=@{}
function New-TestBlockMap([string]$Name,[string]$Hash,[int]$CompressedSize,[bool]$ChangeOther=$false,[int]$LocalHeaderSize=46,[bool]$AddComment=$false,[int]$UncompressedSize=$size,[bool]$ReverseFiles=$false,[string]$OtherFileName='other.bin',[bool]$EmptyOtherFile=$false,[bool]$TwoOtherBlocks=$false,[bool]$SwapOtherBlocks=$false) {
    <# XMLWriter owns syntax; literal expected policy decisions stay below. #>
    $path=Join-Path $fixture ($Name+'.xml')
    $settings=[Xml.XmlWriterSettings]::new();$settings.Encoding=[Text.UTF8Encoding]::new($false)
    $writer=[Xml.XmlWriter]::Create($path,$settings)
    try {
        $writer.WriteStartDocument($false)
        $writer.WriteStartElement('BlockMap','http://schemas.microsoft.com/appx/2010/blockmap')
        $writer.WriteAttributeString('HashMethod','http://www.w3.org/2001/04/xmlenc#sha256')
        # Native writer actions supply two independently specified entries. The
        # explicit index order changes only complete File sibling positions.
        $fileWriters=@({
        $writer.WriteStartElement('File');$writer.WriteAttributeString('Name','JpgSpinner.winmd')
        $writer.WriteAttributeString('Size',$UncompressedSize.ToString([Globalization.CultureInfo]::InvariantCulture))
        $writer.WriteAttributeString('LfhSize',$LocalHeaderSize.ToString([Globalization.CultureInfo]::InvariantCulture))
        $writer.WriteStartElement('Block');$writer.WriteAttributeString('Hash',$Hash)
        $writer.WriteAttributeString('Size',$CompressedSize.ToString([Globalization.CultureInfo]::InvariantCulture))
        $writer.WriteEndElement();$writer.WriteEndElement()
        },{
        $writer.WriteStartElement('File');$writer.WriteAttributeString('Name',$OtherFileName)
        $writer.WriteAttributeString('Size',$(if($EmptyOtherFile){'0'}elseif($TwoOtherBlocks){'65537'}else{'1'}))
        $writer.WriteAttributeString('LfhSize',([Text.Encoding]::UTF8.GetByteCount($OtherFileName)+30).ToString([Globalization.CultureInfo]::InvariantCulture))
        if(-not $EmptyOtherFile){
            $otherHashes=@([Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([byte[]]@([byte]$ChangeOther))))
            if($TwoOtherBlocks){$otherHashes+=@([Convert]::ToBase64String([Security.Cryptography.SHA256]::HashData([byte[]]@(2))))}
            if($SwapOtherBlocks){[Array]::Reverse($otherHashes)}
            foreach($otherHash in $otherHashes){$writer.WriteStartElement('Block');$writer.WriteAttributeString('Hash',$otherHash);$writer.WriteEndElement()}
        }
        $writer.WriteEndElement()
        })
        $fileOrder=if($ReverseFiles){@(1,0)}else{@(0,1)}
        foreach($fileIndex in $fileOrder){& $fileWriters[$fileIndex]}
        if($AddComment){$writer.WriteComment('unapproved unrelated XML change')}
        $writer.WriteEndElement();$writer.WriteEndDocument()
    } finally {$writer.Dispose()}
    $before[$path]=(Get-FileHash -LiteralPath $path).Hash
    return $path
}
$firstMap=New-TestBlockMap first $firstHash 300
$secondMap=New-TestBlockMap second $secondHash 301
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
$reorderedMap=New-TestBlockMap reordered $firstHash 300 -ReverseFiles $true
if((Get-FileHash -LiteralPath $reorderedMap).Hash -ceq (Get-FileHash -LiteralPath $firstMap).Hash){throw 'File-order fixture must actually differ.'}
$reordered=& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $reorderedMap -FirstWinmdPath $firstWinmd -SecondWinmdPath $firstWinmd
if($reordered.ByteIdentical -or -not $reordered.FileOrderVarianceObserved -or $reordered.ChangedBlocks.Count -ne 0){throw 'Whole-File permutation was refused or concealed.'}
$reorderedIdentityMap=New-TestBlockMap reordered-identity $secondHash 1000 -ReverseFiles $true
$reorderedIdentity=& $test -FirstBlockMapPath $firstMap -SecondBlockMapPath $reorderedIdentityMap -FirstWinmdPath $firstWinmd -SecondWinmdPath $secondWinmd
if($reorderedIdentity.ByteIdentical -or -not $reorderedIdentity.FileOrderVarianceObserved -or $reorderedIdentity.ChangedBlocks.Count -ne 1){throw 'Combined whole-File and qualified identity variance was refused or concealed.'}
foreach($case in @(@{Name='unicode';OtherFileName='фото.bin';Empty=$false},@{Name='empty';OtherFileName='empty.bin';Empty=$true})){
    $original=New-TestBlockMap ($case.Name+'-first') $firstHash 300 -OtherFileName $case.OtherFileName -EmptyOtherFile $case.Empty
    $permuted=New-TestBlockMap ($case.Name+'-second') $firstHash 300 -ReverseFiles $true -OtherFileName $case.OtherFileName -EmptyOtherFile $case.Empty
    $result=& $test -FirstBlockMapPath $original -SecondBlockMapPath $permuted -FirstWinmdPath $firstWinmd -SecondWinmdPath $firstWinmd
    if($result.ByteIdentical -or -not $result.FileOrderVarianceObserved){throw 'Native UTF-8/empty whole-File permutation was refused or concealed.'}
}
# Equal concatenated envelope bytes are insufficient: moving a space from
# between two File entries to after them is not a permutation of whole entries.
$betweenPath=Join-Path $fixture envelope-between.xml
$afterPath=Join-Path $fixture envelope-after.xml
$sourceText=[IO.File]::ReadAllText($firstMap)
if(-not $sourceText.Contains('</File><File',[StringComparison]::Ordinal)){throw 'Native fixture lacks adjacent complete File entries.'}
[IO.File]::WriteAllText($betweenPath,$sourceText.Replace('</File><File','</File> <File',[StringComparison]::Ordinal))
[IO.File]::WriteAllText($afterPath,$sourceText.Replace('</BlockMap>',' </BlockMap>',[StringComparison]::Ordinal))
foreach($path in @($betweenPath,$afterPath)){$before[$path]=(Get-FileHash -LiteralPath $path).Hash}
$message=$null
try {& $test -FirstBlockMapPath $betweenPath -SecondBlockMapPath $afterPath -FirstWinmdPath $firstWinmd -SecondWinmdPath $firstWinmd | Out-Null} catch {$message=$_.Exception.Message}
if($null -eq $message){throw 'Redistributed envelope whitespace passed as a whole-File permutation.'}
# Native DOM mutations construct membership controls, never expected outcomes.
# Every control also reorders files so an order-only fast path cannot hide it.
foreach($control in @('duplicate','missing','extra','file-attribute-order','block-order','file-quotes','envelope-quotes')){
    $path=Join-Path $fixture ($control+'.xml')
    $template=New-TestBlockMap ($control+'-template') $secondHash 301 -ReverseFiles $true -TwoOtherBlocks $true
    $document=[Xml.XmlDocument]::new();$document.XmlResolver=$null;$document.PreserveWhitespace=$true
    $document.LoadXml([IO.File]::ReadAllText($template))
    $other=$document.DocumentElement.FirstChild
    if($other.GetAttribute('Name') -cne 'other.bin' -or $other.ChildNodes.Count -ne 2){throw 'Native order-control fixture has incorrect preconditions.'}
    switch($control){
        'duplicate' {[void]$document.DocumentElement.AppendChild($other.CloneNode($true))}
        'missing' {[void]$document.DocumentElement.RemoveChild($other)}
        'extra' {$extra=$other.CloneNode($true);$extra.SetAttribute('Name','extra.bin');[void]$document.DocumentElement.AppendChild($extra)}
        'file-attribute-order' {$name=$other.GetAttribute('Name');$other.RemoveAttribute('Name');$other.SetAttribute('Name',$name)}
        'block-order' {[void]$other.AppendChild($other.FirstChild)}
    }
    $settings=[Xml.XmlWriterSettings]::new();$settings.Encoding=[Text.UTF8Encoding]::new($false)
    $writer=[Xml.XmlWriter]::Create($path,$settings)
    try{$document.Save($writer)}finally{$writer.Dispose()}
    if($control -ceq 'file-quotes'){
        $text=[IO.File]::ReadAllText($path)
        if(-not $text.Contains('LfhSize="39"',[StringComparison]::Ordinal)){throw 'Literal quote-control precondition is absent.'}
        [IO.File]::WriteAllText($path,$text.Replace('LfhSize="39"',"LfhSize='39'",[StringComparison]::Ordinal))
    }
    if($control -ceq 'envelope-quotes'){
        $text=[IO.File]::ReadAllText($path)
        [IO.File]::WriteAllText($path,$text.Replace('HashMethod="http://www.w3.org/2001/04/xmlenc#sha256"',"HashMethod='http://www.w3.org/2001/04/xmlenc#sha256'",[StringComparison]::Ordinal))
    }
    $before[$path]=(Get-FileHash -LiteralPath $path).Hash
    $baseline=New-TestBlockMap ($control+'-baseline') $firstHash 300 -TwoOtherBlocks $true
    $message=$null
    try{& $test -FirstBlockMapPath $baseline -SecondBlockMapPath $path -FirstWinmdPath $firstWinmd -SecondWinmdPath $secondWinmd | Out-Null}catch{$message=$_.Exception.Message}
    if($null -eq $message){throw "Whole-File permutation concealed an unapproved change: $control"}
}
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
Write-Output 'PASS: whole-File order and qualified identity fields accepted truthfully; membership, ordered blocks, literal bytes/envelope and unchanged-hash size controls refused; inputs unchanged.'
