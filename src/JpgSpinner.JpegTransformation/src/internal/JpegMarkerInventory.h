#pragma once

#include <jpg_spinner/domain/ImageDimensions.h>
#include <jpg_spinner/domain/JpegFrameProperties.h>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace jpg_spinner::jpeg::internal
{
/// Source-relative byte range that proves containment without adding its two
/// untrusted operands. The scanner stores ranges rather than borrowed spans so
/// an inventory remains valid after the caller releases the encoded bytes.
struct EncodedByteRange final
{
    std::uint64_t offsetBytes;
    std::uint64_t lengthBytes;

    [[nodiscard]] constexpr bool isContainedWithin(const std::uint64_t sourceLengthBytes) const noexcept
    {
        return offsetBytes <= sourceLengthBytes && lengthBytes <= sourceLengthBytes - offsetBytes;
    }

    [[nodiscard]] friend constexpr bool operator==(const EncodedByteRange &,
                                                   const EncodedByteRange &) noexcept = default;
};

struct JpegFrameHeader final
{
    jpg_spinner::domain::JpegCodingProcess codingProcess;
    std::uint8_t samplePrecisionBits;
    jpg_spinner::domain::ImageDimensions dimensions;
    std::vector<jpg_spinner::domain::JpegComponentDescription> components;
    EncodedByteRange encodedRange;

    [[nodiscard]] friend bool operator==(const JpegFrameHeader &, const JpegFrameHeader &) = default;
};

/// Reference to one marker in source order. payloadRange excludes the marker
/// code and the marker segment's two-byte length field. Standalone markers
/// therefore have a zero-length payload immediately after their marker code.
struct JpegMarkerReference final
{
    std::uint8_t markerCode;
    EncodedByteRange encodedRange;
    EncodedByteRange payloadRange;

    [[nodiscard]] friend constexpr bool operator==(const JpegMarkerReference &,
                                                   const JpegMarkerReference &) noexcept = default;
};

/// One ICC APP2 profile fragment. The source range excludes the 12-byte ICC
/// identifier and the sequence/count bytes, so later preservation code can
/// assemble the profile without interpreting its internal color data.
struct IccProfileChunkReference final
{
    std::uint8_t sequenceNumber;
    std::uint8_t chunkCount;
    EncodedByteRange profileDataRange;

    [[nodiscard]] friend constexpr bool operator==(const IccProfileChunkReference &,
                                                   const IccProfileChunkReference &) noexcept = default;
};

/// Adobe Extended XMP identifies all chunks of one extension serialization by
/// the uppercase hexadecimal MD5 text stored in each APP1 segment.
struct ExtendedXmpGuid final
{
    std::array<char, 32> uppercaseMd5HexadecimalCharacters;

    [[nodiscard]] friend constexpr bool operator==(const ExtendedXmpGuid &, const ExtendedXmpGuid &) noexcept = default;
};

struct ExtendedXmpChunkReference final
{
    ExtendedXmpGuid packetGuid;
    std::uint32_t completePacketLengthBytes;
    std::uint32_t chunkOffsetBytes;
    EncodedByteRange packetDataRange;

    [[nodiscard]] friend constexpr bool operator==(const ExtendedXmpChunkReference &,
                                                   const ExtendedXmpChunkReference &) noexcept = default;
};

/// Application-owned metadata payload references discovered during the outer
/// JPEG walk. Each range excludes its namespace/header bytes, and all vectors
/// retain source order unless a format defines an explicit logical order.
struct JpegMetadataSegmentInventory final
{
    std::vector<EncodedByteRange> exifTiffDataRanges;
    std::vector<EncodedByteRange> standardXmpPacketRanges;
    std::vector<IccProfileChunkReference> iccProfileChunks;
    std::vector<ExtendedXmpChunkReference> extendedXmpChunks;

    [[nodiscard]] friend bool operator==(const JpegMetadataSegmentInventory &,
                                         const JpegMetadataSegmentInventory &) = default;
};

/// Immutable structural result of the bounded marker walk. Its constructor is
/// public only because this header is an internal module seam; application
/// callers cannot include it through the module's public include directory.
class JpegMarkerInventory final
{
  public:
    JpegMarkerInventory(std::uint64_t encodedSourceLengthBytes, JpegFrameHeader frameHeader, std::uint32_t scanCount,
                        std::uint64_t metadataEncodedLengthBytes, std::uint64_t endOfImageMarkerOffsetBytes,
                        std::vector<JpegMarkerReference> markers, std::vector<EncodedByteRange> entropyCodedDataRanges,
                        JpegMetadataSegmentInventory metadataSegments) noexcept
        : encodedSourceLengthBytes_(encodedSourceLengthBytes), frameHeader_(std::move(frameHeader)),
          scanCount_(scanCount), metadataEncodedLengthBytes_(metadataEncodedLengthBytes),
          endOfImageMarkerOffsetBytes_(endOfImageMarkerOffsetBytes), markers_(std::move(markers)),
          entropyCodedDataRanges_(std::move(entropyCodedDataRanges)), metadataSegments_(std::move(metadataSegments))
    {
    }

    [[nodiscard]] std::uint64_t encodedSourceLengthBytes() const noexcept
    {
        return encodedSourceLengthBytes_;
    }

    [[nodiscard]] const JpegFrameHeader &frameHeader() const noexcept
    {
        return frameHeader_;
    }

    [[nodiscard]] std::uint32_t scanCount() const noexcept
    {
        return scanCount_;
    }

    /// Total encoded bytes occupied by APPn and COM marker segments,
    /// including their marker and length fields. Including framing prevents a
    /// stream of empty metadata markers from bypassing the aggregate bound.
    [[nodiscard]] std::uint64_t metadataEncodedLengthBytes() const noexcept
    {
        return metadataEncodedLengthBytes_;
    }

    [[nodiscard]] std::uint64_t endOfImageMarkerOffsetBytes() const noexcept
    {
        return endOfImageMarkerOffsetBytes_;
    }

    /// Bytes outside the first JPEG codestream (T.81 B.2.1). No byte value is
    /// implicitly disposable padding. The metadata analyzer must interpret
    /// container declarations and reject unsupported appended assets before
    /// transformation; structural scan success alone is not transform approval.
    [[nodiscard]] EncodedByteRange trailingDataRange() const noexcept
    {
        // The scanner proves that the complete two-byte EOI is contained.
        const auto firstTrailingByteOffset = endOfImageMarkerOffsetBytes_ + 2;
        return {firstTrailingByteOffset, encodedSourceLengthBytes_ - firstTrailingByteOffset};
    }

    [[nodiscard]] const std::vector<JpegMarkerReference> &markers() const noexcept
    {
        return markers_;
    }

    [[nodiscard]] const std::vector<EncodedByteRange> &entropyCodedDataRanges() const noexcept
    {
        return entropyCodedDataRanges_;
    }

    /// ICC chunks are returned in logical 1-based sequence order even when
    /// APP2 markers were physically reordered in the source JPEG.
    [[nodiscard]] const std::vector<IccProfileChunkReference> &iccProfileChunks() const noexcept
    {
        return metadataSegments_.iccProfileChunks;
    }

    [[nodiscard]] const std::vector<EncodedByteRange> &exifTiffDataRanges() const noexcept
    {
        return metadataSegments_.exifTiffDataRanges;
    }

    [[nodiscard]] const std::vector<EncodedByteRange> &standardXmpPacketRanges() const noexcept
    {
        return metadataSegments_.standardXmpPacketRanges;
    }

    /// Extended XMP chunks are returned in logical offset order, independent
    /// of their physical APP1 marker order.
    [[nodiscard]] const std::vector<ExtendedXmpChunkReference> &extendedXmpChunks() const noexcept
    {
        return metadataSegments_.extendedXmpChunks;
    }

    [[nodiscard]] friend bool operator==(const JpegMarkerInventory &, const JpegMarkerInventory &) = default;

  private:
    std::uint64_t encodedSourceLengthBytes_;
    JpegFrameHeader frameHeader_;
    std::uint32_t scanCount_;
    std::uint64_t metadataEncodedLengthBytes_;
    std::uint64_t endOfImageMarkerOffsetBytes_;
    std::vector<JpegMarkerReference> markers_;
    std::vector<EncodedByteRange> entropyCodedDataRanges_;
    JpegMetadataSegmentInventory metadataSegments_;
};
} // namespace jpg_spinner::jpeg::internal
