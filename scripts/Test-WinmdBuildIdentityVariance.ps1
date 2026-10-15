<# Read-only comparison under the owner-approved native WinMD identity policy.
   PEReader/MetadataReader own every format boundary; only their timestamp and
   single Module MVID may vary. This is not full byte reproducibility, metadata
   rewriting or general semantic equivalence. Original files are never written.
   See docs/implementation/2026-10-15-package-payload-comparison-contract.md. #>
[CmdletBinding()]
param([Parameter(Mandatory)][string]$FirstWinmdPath,[Parameter(Mandatory)][string]$SecondWinmdPath)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
function Read-WinmdBuildIdentity([string]$Path) {
    <# Snapshot one ordinary qualified app WinMD and locate identity fields via
       native readers. The 16 MiB ceiling bounds this narrow release inspection;
       current native app metadata is below 32 KiB, not an unbounded input API. #>
    $path=$ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Path)
    $ancestor=$path
    while (-not [string]::IsNullOrEmpty($ancestor)) {
        if ((Test-Path -LiteralPath $ancestor) -and ([IO.File]::GetAttributes($ancestor) -band [IO.FileAttributes]::ReparsePoint) -ne 0) {
            throw 'WinMD comparison refuses redirected files or ancestors.'
        }
        $ancestor=[IO.Path]::GetDirectoryName($ancestor)
    }
    $file=[IO.File]::OpenRead($path)
    try {
        if ($file.Length -le 0 -or $file.Length -gt 16MB) {throw 'WinMD is outside the qualified inspection size bound.'}
        $bytes=[byte[]]::new([int]$file.Length)
        $file.ReadExactly($bytes)
    } finally {$file.Dispose()}
    $stream=[IO.MemoryStream]::new($bytes,$false)
    try {
        $pe=[Reflection.PortableExecutable.PEReader]::new($stream)
        try {
            $metadata=[Reflection.Metadata.PEReaderExtensions]::GetMetadataReader($pe,[Reflection.Metadata.MetadataReaderOptions]::None)
            if (-not $metadata.IsAssembly -or
                ($metadata.GetAssemblyDefinition().Flags -band [Reflection.AssemblyFlags]::WindowsRuntime) -eq 0 -or
                [Reflection.Metadata.Ecma335.MetadataReaderExtensions]::GetTableRowCount($metadata,[Reflection.Metadata.Ecma335.TableIndex]::Module) -ne 1) {
                throw 'WinMD comparison requires one native Windows Runtime metadata module.'
            }
            $module=$metadata.GetModuleDefinition()
            $guidBytes=[Reflection.Metadata.Ecma335.MetadataReaderExtensions]::GetHeapSize($metadata,[Reflection.Metadata.Ecma335.HeapIndex]::Guid)
            $guidIndex=[Reflection.Metadata.Ecma335.MetadataTokens]::GetHeapOffset($module.Mvid)
            # The qualified SDK producer has exactly one GUID, used only for
            # its MVID. Reject edit-and-continue IDs or an ambiguous heap instead
            # of allowing another identity to change through an aliased slot.
            if ($guidBytes -ne 16 -or $guidIndex -ne 1 -or $module.Generation -ne 0 -or
                -not $module.GenerationId.IsNil -or -not $module.BaseGenerationId.IsNil) {
                throw 'WinMD build identity is missing or outside the qualified single-MVID layout.'
            }
            # GetHeapOffset(GuidHandle) is a ONE-BASED GUID index, not a byte
            # offset. Native heap offsets are relative to the metadata start.
            # https://learn.microsoft.com/dotnet/api/system.reflection.metadata.ecma335.metadatatokens.getheapoffset
            $mvidOffset=$pe.PEHeaders.MetadataStartOffset+
                [Reflection.Metadata.Ecma335.MetadataReaderExtensions]::GetHeapMetadataOffset($metadata,[Reflection.Metadata.Ecma335.HeapIndex]::Guid)+16*($guidIndex-1)
            # PE/COFF defines TimeDateStamp at byte four of its 20-byte header.
            # PEReader, not an image-specific constant, locates that header.
            # https://learn.microsoft.com/windows/win32/debug/pe-format#coff-file-header-object-and-image
            $timestampOffset=$pe.PEHeaders.CoffHeaderStartOffset+4
            if ($pe.PEHeaders.CoffHeaderStartOffset -lt 0 -or $pe.PEHeaders.MetadataStartOffset -lt 0 -or
                $timestampOffset -lt 0 -or $mvidOffset -lt $timestampOffset+4 -or $mvidOffset+16 -gt $bytes.Length) {
                throw 'WinMD identity ranges are invalid or overlap.'
            }
            [pscustomobject]@{
                Bytes=$bytes;TimestampOffset=$timestampOffset;MvidOffset=$mvidOffset
                Timestamp=[BitConverter]::ToUInt32($bytes,$timestampOffset);Mvid=$metadata.GetGuid($module.Mvid)
                Sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
            }
        } finally {$pe.Dispose()}
    } finally {$stream.Dispose()}
}
function Test-ByteIntervalEquality([byte[]]$First,[byte[]]$Second,[int]$Start,[int]$Length) {
    <# Compare an unchanged interval without altering either original snapshot. #>
    $left=[Linq.Enumerable]::Take[byte]([Linq.Enumerable]::Skip[byte]($First,$Start),$Length)
    $right=[Linq.Enumerable]::Take[byte]([Linq.Enumerable]::Skip[byte]($Second,$Start),$Length)
    return [Linq.Enumerable]::SequenceEqual[byte]($left,$right)
}
$first=Read-WinmdBuildIdentity $FirstWinmdPath
$second=Read-WinmdBuildIdentity $SecondWinmdPath
if ($first.Bytes.Length -ne $second.Bytes.Length -or $first.TimestampOffset -ne $second.TimestampOffset -or $first.MvidOffset -ne $second.MvidOffset) {
    throw 'WinMD length or native identity layout differs.'
}
$intervals=@(
    @{Start=0;Length=$first.TimestampOffset},
    @{Start=($first.TimestampOffset+4);Length=($first.MvidOffset-$first.TimestampOffset-4)},
    @{Start=($first.MvidOffset+16);Length=($first.Bytes.Length-$first.MvidOffset-16)}
)
foreach ($interval in $intervals) {
    if (-not (Test-ByteIntervalEquality $first.Bytes $second.Bytes $interval.Start $interval.Length)) {
        throw 'WinMD differs outside the accepted build-identity fields.'
    }
}
$byteIdentical=[Linq.Enumerable]::SequenceEqual[byte]($first.Bytes,$second.Bytes)
[pscustomobject]@{
    Policy='windows-sdk-winmd-build-identity-v1';ByteIdentical=$byteIdentical;IdentityVarianceObserved=(-not $byteIdentical)
    FirstSha256=$first.Sha256;SecondSha256=$second.Sha256
    FirstTimestamp=$first.Timestamp;SecondTimestamp=$second.Timestamp;FirstMvid=$first.Mvid;SecondMvid=$second.Mvid
    TimestampOffset=$first.TimestampOffset;MvidOffset=$first.MvidOffset
}
