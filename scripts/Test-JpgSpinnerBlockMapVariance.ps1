<# Read-only comparison of already native-validated package snapshots. Allow
   only hash/compressed-size consequences on the app WinMD's qualified identity
   blocks, plus permutation of complete File siblings. XMLReader owns syntax
   and source boundaries; original containers must separately pass
   MakeAppx validation. This predicate neither authenticates nor repairs them. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FirstBlockMapPath,
    [Parameter(Mandatory)][string]$SecondBlockMapPath,
    [Parameter(Mandatory)][string]$FirstWinmdPath,
    [Parameter(Mandatory)][string]$SecondWinmdPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Get-AttributeByteRange([Xml.XmlReader]$Reader,[string]$Text,$LineStarts) {
    <# Use native parser coordinates to retain literal byte strictness outside
       the selected values. Deep XML equality would also permit attribute-order
       changes. Reject an unqualified lexical form instead of guessing a span. #>
    $line=[Xml.IXmlLineInfo]$Reader
    if (-not $line.HasLineInfo() -or $line.LineNumber -le 0 -or $line.LineNumber -gt $LineStarts.Count) {throw 'Block-map parser did not locate an attribute.'}
    $start=$LineStarts[$line.LineNumber-1].Index+$line.LinePosition-1
    $literal=$Reader.Name+'="'+$Reader.Value+'"'
    if ($start -lt 0 -or $start+$literal.Length -gt $Text.Length -or
        -not [string]::Equals($Text.Substring($start,$literal.Length),$literal,[StringComparison]::Ordinal)) {
        throw 'Block-map attribute spelling is outside the qualified native SDK layout.'
    }
    # XmlReader coordinates count UTF-16 characters; the package's raw XML is
    # UTF-8. The native encoder owns conversion, including non-ASCII prefixes.
    [pscustomobject]@{
        Start=[Text.Encoding]::UTF8.GetByteCount($Text.Substring(0,$start+$Reader.Name.Length+2))
        Length=[Text.Encoding]::UTF8.GetByteCount($Reader.Value)
    }
}
function Get-ElementMarkupByteRange([Xml.XmlReader]$Reader,[string]$Text,$LineStarts) {
    <# Locate native-reader markup, verifying the SDK's literal spelling before
       converting UTF-16 coordinates to original UTF-8 offsets. Never search for
       tags or serialize XML: either could conceal unapproved lexical changes. #>
    $line=[Xml.IXmlLineInfo]$Reader
    if(-not $line.HasLineInfo() -or $line.LineNumber -le 0 -or $line.LineNumber -gt $LineStarts.Count){throw 'Native XML reader did not locate File markup.'}
    $closing=$Reader.NodeType -eq [Xml.XmlNodeType]::EndElement
    $prefix=if($closing){'</'}else{'<'}
    $start=$LineStarts[$line.LineNumber-1].Index+$line.LinePosition-1-$prefix.Length
    $literal=$prefix+$Reader.Name+$(if($closing){'>'}else{''})
    if($start -lt 0 -or $start+$literal.Length -gt $Text.Length -or
        -not [string]::Equals($Text.Substring($start,$literal.Length),$literal,[StringComparison]::Ordinal)){
        throw 'File markup is outside the qualified native SDK spelling.'
    }
    [pscustomobject]@{Start=[Text.Encoding]::UTF8.GetByteCount($Text.Substring(0,$start));Length=[Text.Encoding]::UTF8.GetByteCount($literal)}
}
function Read-BlockMapComparisonSnapshot([string]$Path) {
    <# Snapshot a bounded ordinary block map. Native XML boundaries locate whole
       File byte ranges; the envelope and each range retain their original bytes.
       Only the exact app File contributes candidate identity-block intervals. #>
    $path=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {throw 'Block-map comparison refuses redirected inputs.'}
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
    $file=[IO.File]::OpenRead($path)
    try {
        if ($file.Length -le 0 -or $file.Length -gt 1MB) {throw 'Block map exceeds the qualified inspection bound.'}
        $bytes=[byte[]]::new([int]$file.Length);$file.ReadExactly($bytes)
    } finally {$file.Dispose()}
    $text=[Text.UTF8Encoding]::new($false,$true).GetString($bytes)
    # This maps native line coordinates, not XML tokens. CR-only line endings
    # are outside the qualified MakeAppx representation and refuse below.
    $lineStarts=[regex]::Matches($text,'(?m)^')
    $settings=[Xml.XmlReaderSettings]::new()
    $settings.DtdProcessing=[Xml.DtdProcessing]::Prohibit;$settings.XmlResolver=$null
    $settings.MaxCharactersInDocument=1MB
    $stream=[IO.MemoryStream]::new($bytes,$false)
    $appWinmdBlocks=[Collections.Generic.List[object]]::new();$appWinmdFileCount=0;$inAppWinmdFile=$false
    $fileByteRanges=[Collections.Generic.SortedDictionary[string,object]]::new([StringComparer]::Ordinal)
    $fileOrder=[Collections.Generic.List[string]]::new();$currentFileByteRange=$null
    try {
        $reader=[Xml.XmlReader]::Create($stream,$settings)
        try {
            while ($reader.Read()) {
                if ($reader.NodeType -eq [Xml.XmlNodeType]::Element -and $reader.Depth -eq 0 -and
                    ($reader.LocalName -cne 'BlockMap' -or $reader.NamespaceURI -cne 'http://schemas.microsoft.com/appx/2010/blockmap' -or
                     $reader.GetAttribute('HashMethod') -cne 'http://www.w3.org/2001/04/xmlenc#sha256')) {
                    throw 'Block map is outside the qualified native SHA-256 package contract.'
                }
                if ($reader.NodeType -eq [Xml.XmlNodeType]::Element -and $reader.Depth -eq 1) {
                    if($reader.LocalName -ceq 'File' -and $reader.NamespaceURI -ceq 'http://schemas.microsoft.com/appx/2010/blockmap'){
                        $name=$reader.GetAttribute('Name')
                        if([string]::IsNullOrEmpty($name) -or $fileByteRanges.ContainsKey($name)){throw 'Block map File names must be nonempty and unique.'}
                        $opening=Get-ElementMarkupByteRange $reader $text $lineStarts
                        $currentFileByteRange=[pscustomobject]@{Name=$name;Start=$opening.Start;Length=0}
                        $fileByteRanges.Add($name,$currentFileByteRange);$fileOrder.Add($name)
                        if($reader.IsEmptyElement){
                            # Zero-byte files use a self-closing native SDK
                            # element. Locate its last attribute via the reader,
                            # then verify the exact terminator; do not scan XML.
                            if($reader.AttributeCount -eq 0){throw 'Empty File lacks native attributes.'}
                            $reader.MoveToAttribute($reader.AttributeCount-1)
                            $last=Get-AttributeByteRange $reader $text $lineStarts
                            [void]$reader.MoveToElement()
                            $end=$last.Start+$last.Length+1
                            $suffix=if($end+3 -le $bytes.Length -and [Text.Encoding]::UTF8.GetString($bytes,$end,3) -ceq ' />'){' />'}else{'/>'}
                            if($end+$suffix.Length -gt $bytes.Length -or [Text.Encoding]::UTF8.GetString($bytes,$end,$suffix.Length) -cne $suffix){throw 'Empty File terminator is outside the qualified native SDK spelling.'}
                            $currentFileByteRange.Length=$end+$suffix.Length-$currentFileByteRange.Start
                            $currentFileByteRange=$null
                        }
                    }
                    $inAppWinmdFile=$reader.LocalName -ceq 'File' -and
                        $reader.NamespaceURI -ceq 'http://schemas.microsoft.com/appx/2010/blockmap' -and
                        $reader.GetAttribute('Name') -ceq 'JpgSpinner.winmd'
                    if ($inAppWinmdFile) {$appWinmdFileCount++}
                }
                if ($inAppWinmdFile -and $reader.NodeType -eq [Xml.XmlNodeType]::Element -and $reader.Depth -eq 2 -and
                    $reader.LocalName -ceq 'Block' -and $reader.NamespaceURI -ceq 'http://schemas.microsoft.com/appx/2010/blockmap') {
                    if (-not $reader.MoveToAttribute('Hash')) {throw 'App WinMD block lacks its native hash.'}
                    $hash=$reader.Value;$hashRange=Get-AttributeByteRange $reader $text $lineStarts
                    # Native scalar consumers keep invalid/ambiguous lexical
                    # values out of comparison evidence. They do not replace
                    # the outer MakeAppx package/hash validation requirement.
                    $decodedHash=[Convert]::FromBase64String($hash)
                    if ($decodedHash.Length -ne 32 -or [Convert]::ToBase64String($decodedHash) -cne $hash) {throw 'App WinMD block hash is outside the qualified SHA-256 representation.'}
                    [void]$reader.MoveToElement()
                    $size=$null;$sizeRange=$null
                    if ($reader.MoveToAttribute('Size')) {
                        $size=$reader.Value
                        $sizeBytes=[Xml.XmlConvert]::ToInt64($size)
                        if ($sizeBytes -le 0 -or $sizeBytes.ToString([Globalization.CultureInfo]::InvariantCulture) -cne $size) {throw 'Compressed block size is outside the qualified native integer representation.'}
                        $sizeRange=Get-AttributeByteRange $reader $text $lineStarts
                        [void]$reader.MoveToElement()
                    }
                    $appWinmdBlocks.Add([pscustomobject]@{Hash=$hash;HashRange=$hashRange;Size=$size;SizeRange=$sizeRange})
                }
                if ($reader.NodeType -eq [Xml.XmlNodeType]::EndElement -and $reader.Depth -eq 1) {
                    if($null -ne $currentFileByteRange){
                        $closing=Get-ElementMarkupByteRange $reader $text $lineStarts
                        $currentFileByteRange.Length=$closing.Start+$closing.Length-$currentFileByteRange.Start
                        $currentFileByteRange=$null
                    }
                    $inAppWinmdFile=$false
                }
            }
        } finally {$reader.Dispose()}
    } finally {$stream.Dispose()}
    if ($appWinmdFileCount -ne 1 -or $appWinmdBlocks.Count -eq 0) {throw 'Block map requires exactly one nonempty app WinMD file entry.'}
    # Keep each envelope gap at its original sibling slot, not concatenated:
    # concatenation could conceal redistribution of whitespace/comments across
    # slots. Only complete File ranges may move. No XML is serialized/rewritten.
    $envelopeGaps=[Collections.Generic.List[byte[]]]::new();$offset=0
    foreach($name in $fileOrder){
        $entry=$fileByteRanges[$name]
        if($entry.Length -le 0 -or $entry.Start -lt $offset -or $entry.Start+$entry.Length -gt $bytes.Length){throw 'Native File byte ranges are incomplete or overlap.'}
        $gap=[byte[]]::new($entry.Start-$offset)
        [Array]::Copy($bytes,$offset,$gap,0,$gap.Length)
        $envelopeGaps.Add($gap)
        $offset=$entry.Start+$entry.Length
    }
    $gap=[byte[]]::new($bytes.Length-$offset)
    [Array]::Copy($bytes,$offset,$gap,0,$gap.Length)
    $envelopeGaps.Add($gap)
    [pscustomobject]@{Bytes=$bytes;AppWinmdBlocks=$appWinmdBlocks.ToArray();FileByteRanges=$fileByteRanges;FileOrder=$fileOrder.ToArray();EnvelopeGaps=$envelopeGaps.ToArray()}
}
$identity=& (Join-Path $PSScriptRoot 'Test-WinmdBuildIdentityVariance.ps1') -FirstWinmdPath $FirstWinmdPath -SecondWinmdPath $SecondWinmdPath
$first=Read-BlockMapComparisonSnapshot $FirstBlockMapPath
$second=Read-BlockMapComparisonSnapshot $SecondBlockMapPath
if(-not [Linq.Enumerable]::SequenceEqual[string]($first.FileByteRanges.Keys,$second.FileByteRanges.Keys,[StringComparer]::Ordinal)){throw 'Block map File membership differs.'}
for($gapIndex=0;$gapIndex -lt $first.EnvelopeGaps.Count;$gapIndex++){
    if(-not [Linq.Enumerable]::SequenceEqual[byte]($first.EnvelopeGaps[$gapIndex],$second.EnvelopeGaps[$gapIndex])){throw 'Block map document envelope differs.'}
}
if ($first.AppWinmdBlocks.Count -ne $second.AppWinmdBlocks.Count) {throw 'App WinMD block count differs.'}
$possibleChangedBlocks=[Collections.Generic.HashSet[int]]::new()
foreach ($field in @(
    @{Changed=($identity.FirstTimestamp -ne $identity.SecondTimestamp);Start=$identity.TimestampOffset;Length=4},
    @{Changed=($identity.FirstMvid -ne $identity.SecondMvid);Start=$identity.MvidOffset;Length=16}
)) {
    if ($field.Changed) {
        # Microsoft's block-map contract uses 65,536-byte uncompressed blocks.
        # Native package validation establishes actual hashes/compressed sizes;
        # only blocks intersecting changed identity fields are even candidates.
        # https://learn.microsoft.com/uwp/schemas/blockmapschema/element-block
        for ($index=[int][Math]::Floor($field.Start/65536);$index -le [Math]::Floor(($field.Start+$field.Length-1)/65536);$index++) {
            [void]$possibleChangedBlocks.Add($index)
        }
    }
}
$ranges=[Collections.Generic.List[object]]::new();$changed=[Collections.Generic.List[object]]::new()
for ($index=0;$index -lt $first.AppWinmdBlocks.Count;$index++) {
    $left=$first.AppWinmdBlocks[$index];$right=$second.AppWinmdBlocks[$index]
    if (($null -eq $left.Size) -ne ($null -eq $right.Size)) {throw 'Block compression representation differs.'}
    if ($left.Hash -ceq $right.Hash) {
        if ($left.Size -cne $right.Size) {throw 'Compressed size changed on an unchanged-hash block.'}
        continue
    }
    if (-not $possibleChangedBlocks.Contains($index)) {throw 'Block hash changed outside qualified WinMD identity fields.'}
    $ranges.Add(@{First=$left.HashRange;Second=$right.HashRange})
    if ($left.Size -cne $right.Size) {$ranges.Add(@{First=$left.SizeRange;Second=$right.SizeRange})}
    $changed.Add([pscustomobject]@{
        BlockIndex=$index;FirstHashBase64=$left.Hash;SecondHashBase64=$right.Hash
        FirstCompressedSizeBytes=$(if($null -eq $left.Size){$null}else{[Xml.XmlConvert]::ToInt64($left.Size)})
        SecondCompressedSizeBytes=$(if($null -eq $right.Size){$null}else{[Xml.XmlConvert]::ToInt64($right.Size)})
    })
}
# Compare whole File entries by exact name, never block order. The reader owns
# boundaries; every original per-file byte remains strict except the approved
# scalar intervals. Separate offsets retain size values with different widths.
foreach($name in $first.FileByteRanges.Keys){
    $firstFile=$first.FileByteRanges[$name];$secondFile=$second.FileByteRanges[$name]
    $fileRanges=@(if($name -ceq 'JpgSpinner.winmd'){$ranges | Sort-Object {$_.First.Start}})
    $firstOffset=$firstFile.Start;$secondOffset=$secondFile.Start
    foreach ($range in @($fileRanges)+@(@{First=@{Start=$firstFile.Start+$firstFile.Length;Length=0};Second=@{Start=$secondFile.Start+$secondFile.Length;Length=0}})) {
        $leftCount=$range.First.Start-$firstOffset;$rightCount=$range.Second.Start-$secondOffset
        if ($leftCount -lt 0 -or $leftCount -ne $rightCount -or $range.Second.Start -lt $secondOffset) {throw 'Block map differs outside the accepted WinMD block fields.'}
        $left=[Linq.Enumerable]::Take[byte]([Linq.Enumerable]::Skip[byte]($first.Bytes,$firstOffset),$leftCount)
        $right=[Linq.Enumerable]::Take[byte]([Linq.Enumerable]::Skip[byte]($second.Bytes,$secondOffset),$rightCount)
        if (-not [Linq.Enumerable]::SequenceEqual[byte]($left,$right)) {throw 'Block map differs outside the accepted WinMD block fields.'}
        $firstOffset=$range.First.Start+$range.First.Length;$secondOffset=$range.Second.Start+$range.Second.Length
    }
}
[pscustomobject]@{
    Policy='jpg-spinner-blockmap-qualified-variance-v2'
    ByteIdentical=[Linq.Enumerable]::SequenceEqual[byte]($first.Bytes,$second.Bytes)
    ChangedBlocks=$changed.ToArray();WinmdIdentity=$identity
    FileOrderVarianceObserved=-not [Linq.Enumerable]::SequenceEqual[string]($first.FileOrder,$second.FileOrder,[StringComparer]::Ordinal)
    FirstFileOrder=$first.FileOrder;SecondFileOrder=$second.FileOrder
}
