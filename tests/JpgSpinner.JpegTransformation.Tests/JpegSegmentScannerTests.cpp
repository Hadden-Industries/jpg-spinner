#include "internal/JpegSegmentScanner.h"

#include <jpg_spinner/domain/ImageDimensions.h>
#include <jpg_spinner/domain/ImageProcessingError.h>
#include <jpg_spinner/domain/JpegResourceLimits.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace
{
using jpg_spinner::domain::ImageDimensions;
using jpg_spinner::domain::ImageProcessingErrorCode;
using jpg_spinner::domain::ImageProcessingStage;
using jpg_spinner::domain::JpegResourceLimit;
using jpg_spinner::domain::JpegResourceLimits;
using jpg_spinner::domain::JpegResourceLimitViolation;
using jpg_spinner::domain::PixelHeight;
using jpg_spinner::domain::PixelWidth;
using jpg_spinner::jpeg::internal::EncodedByteRange;
using jpg_spinner::jpeg::internal::JpegCodingProcess;
using jpg_spinner::jpeg::internal::JpegMarkerReference;
using jpg_spinner::jpeg::internal::JpegSegmentScanner;

constexpr std::uint8_t startOfImageMarkerCode = 0xD8;
constexpr std::uint8_t endOfImageMarkerCode = 0xD9;
constexpr std::uint8_t startOfFrameBaselineMarkerCode = 0xC0;
constexpr std::uint8_t startOfScanMarkerCode = 0xDA;
constexpr std::uint8_t applicationSegment1MarkerCode = 0xE1;
constexpr std::uint8_t applicationSegment2MarkerCode = 0xE2;
constexpr std::uint8_t applicationSegment11MarkerCode = 0xEB;
constexpr std::uint8_t commentMarkerCode = 0xFE;
constexpr std::uint8_t restart0MarkerCode = 0xD0;

constexpr std::array<std::uint8_t, 16> c2paManifestStoreTypeUuid{
    0x63, 0x32, 0x70, 0x61, 0x00, 0x11, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
};

constexpr std::array<std::uint8_t, 16> jsonContentTypeUuid{
    0x6A, 0x73, 0x6F, 0x6E, 0x00, 0x11, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
};

struct AppendedMarkerReference final
{
    std::uint8_t markerCode;
    EncodedByteRange encodedRange;
    EncodedByteRange payloadRange;
};

void appendBytes(std::vector<std::byte> &destination, const std::span<const std::uint8_t> bytes)
{
    for (const auto byte : bytes)
    {
        destination.push_back(static_cast<std::byte>(byte));
    }
}

void appendBytes(std::vector<std::byte> &destination, const std::initializer_list<std::uint8_t> bytes)
{
    appendBytes(destination, std::span(bytes.begin(), bytes.size()));
}

void appendBigEndianUnsigned16(std::vector<std::byte> &destination, const std::uint16_t value)
{
    destination.push_back(static_cast<std::byte>(value >> 8));
    destination.push_back(static_cast<std::byte>(value & 0xFF));
}

void appendBigEndianUnsigned32(std::vector<std::byte> &destination, const std::uint32_t value)
{
    destination.push_back(static_cast<std::byte>(value >> 24));
    destination.push_back(static_cast<std::byte>((value >> 16) & 0xFF));
    destination.push_back(static_cast<std::byte>((value >> 8) & 0xFF));
    destination.push_back(static_cast<std::byte>(value & 0xFF));
}

void appendBigEndianUnsigned32(std::vector<std::uint8_t> &destination, const std::uint32_t value)
{
    destination.push_back(static_cast<std::uint8_t>(value >> 24));
    destination.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    destination.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    destination.push_back(static_cast<std::uint8_t>(value & 0xFF));
}

AppendedMarkerReference appendStandaloneMarker(std::vector<std::byte> &destination, const std::uint8_t markerCode)
{
    const auto markerOffsetBytes = static_cast<std::uint64_t>(destination.size());
    appendBytes(destination, {0xFF, markerCode});
    return {
        markerCode,
        EncodedByteRange{markerOffsetBytes, 2},
        EncodedByteRange{markerOffsetBytes + 2, 0},
    };
}

AppendedMarkerReference appendLengthBearingMarker(std::vector<std::byte> &destination, const std::uint8_t markerCode,
                                                  const std::span<const std::uint8_t> payload)
{
    const auto markerOffsetBytes = static_cast<std::uint64_t>(destination.size());
    const auto declaredLength = static_cast<std::uint16_t>(payload.size() + 2);
    appendBytes(destination, {0xFF, markerCode});
    appendBigEndianUnsigned16(destination, declaredLength);
    appendBytes(destination, payload);
    return {
        markerCode,
        EncodedByteRange{markerOffsetBytes, static_cast<std::uint64_t>(declaredLength) + 2},
        EncodedByteRange{markerOffsetBytes + 4, static_cast<std::uint64_t>(payload.size())},
    };
}

std::vector<std::uint8_t> createJumbfSuperbox(const std::array<std::uint8_t, 16> &typeUuid,
                                              const std::string_view label)
{
    // ISO/IEC 19566-5 defines a JUMBF superbox as an outer `jumb` box whose
    // first child is a `jumd` description box. Both BMFF lengths include their
    // respective eight-byte length/type headers.
    const auto descriptionBoxLength = static_cast<std::uint32_t>(8 + typeUuid.size() + 1 + label.size() + 1);
    const auto superboxLength = static_cast<std::uint32_t>(8 + descriptionBoxLength);

    std::vector<std::uint8_t> superbox;
    superbox.reserve(superboxLength);
    appendBigEndianUnsigned32(superbox, superboxLength);
    superbox.insert(superbox.end(), {'j', 'u', 'm', 'b'});
    appendBigEndianUnsigned32(superbox, descriptionBoxLength);
    superbox.insert(superbox.end(), {'j', 'u', 'm', 'd'});
    superbox.insert(superbox.end(), typeUuid.begin(), typeUuid.end());
    superbox.push_back(0x03); // Requestable and Label Present, as C2PA 2.4 requires.
    for (const auto character : label)
    {
        superbox.push_back(static_cast<std::uint8_t>(character));
    }
    superbox.push_back(0);
    return superbox;
}

void appendJpegXtBoxAsApp11Segments(std::vector<std::byte> &destination, const std::span<const std::uint8_t> superbox,
                                    const std::uint16_t boxInstanceNumber, const std::size_t firstSegmentBoxByteCount)
{
    REQUIRE(superbox.size() >= 8);
    REQUIRE(firstSegmentBoxByteCount >= 8);
    REQUIRE(firstSegmentBoxByteCount <= superbox.size());

    const auto appendSegment = [&](const std::uint32_t packetSequenceNumber,
                                   const std::span<const std::uint8_t> boxBytes) {
        std::vector<std::uint8_t> payload{
            'J',
            'P',
            static_cast<std::uint8_t>(boxInstanceNumber >> 8),
            static_cast<std::uint8_t>(boxInstanceNumber & 0xFF),
        };
        appendBigEndianUnsigned32(payload, packetSequenceNumber);
        payload.insert(payload.end(), boxBytes.begin(), boxBytes.end());
        appendLengthBearingMarker(destination, applicationSegment11MarkerCode, payload);
    };

    appendSegment(1, superbox.first(firstSegmentBoxByteCount));
    if (firstSegmentBoxByteCount != superbox.size())
    {
        // JPEG XT repeats the box header, including XLBox when LBox is one.
        const auto repeatedHeaderSize = superbox[3] == 1 ? 16 : 8;
        std::vector<std::uint8_t> continuationBytes(superbox.begin(), superbox.begin() + repeatedHeaderSize);
        continuationBytes.insert(continuationBytes.end(),
                                 superbox.begin() + static_cast<std::ptrdiff_t>(firstSegmentBoxByteCount),
                                 superbox.end());
        appendSegment(2, continuationBytes);
    }
}

AppendedMarkerReference appendLengthBearingMarker(std::vector<std::byte> &destination, const std::uint8_t markerCode,
                                                  const std::initializer_list<std::uint8_t> payload)
{
    return appendLengthBearingMarker(destination, markerCode, std::span(payload.begin(), payload.size()));
}

struct AppendedIccProfileChunkReference final
{
    std::uint8_t sequenceNumber;
    std::uint8_t chunkCount;
    EncodedByteRange profileDataRange;
};

AppendedIccProfileChunkReference appendIccProfileChunk(std::vector<std::byte> &destination,
                                                       const std::uint8_t sequenceNumber, const std::uint8_t chunkCount,
                                                       const std::initializer_list<std::uint8_t> profileData)
{
    // ICC Technical Note 10-2021 defines a 12-byte null-terminated
    // "ICC_PROFILE" identifier followed by one-byte sequence/count fields.
    std::vector<std::uint8_t> payload{
        'I', 'C', 'C', '_', 'P', 'R', 'O', 'F', 'I', 'L', 'E', 0, sequenceNumber, chunkCount,
    };
    payload.insert(payload.end(), profileData);
    const auto marker = appendLengthBearingMarker(destination, applicationSegment2MarkerCode, payload);
    return {
        sequenceNumber,
        chunkCount,
        EncodedByteRange{marker.payloadRange.offsetBytes + 14, profileData.size()},
    };
}

EncodedByteRange appendExifTiffData(std::vector<std::byte> &destination,
                                    const std::initializer_list<std::uint8_t> tiffData)
{
    std::vector<std::uint8_t> payload{'E', 'x', 'i', 'f', 0, 0};
    payload.insert(payload.end(), tiffData);
    const auto marker = appendLengthBearingMarker(destination, applicationSegment1MarkerCode, payload);
    return EncodedByteRange{marker.payloadRange.offsetBytes + 6, tiffData.size()};
}

EncodedByteRange appendStandardXmpPacket(std::vector<std::byte> &destination,
                                         const std::initializer_list<std::uint8_t> packetBytes)
{
    constexpr std::string_view standardXmpIdentifier = "http://ns.adobe.com/xap/1.0/";
    std::vector<std::uint8_t> payload;
    payload.reserve(standardXmpIdentifier.size() + 1 + packetBytes.size());
    for (const auto character : standardXmpIdentifier)
    {
        payload.push_back(static_cast<std::uint8_t>(character));
    }
    payload.push_back(0);
    payload.insert(payload.end(), packetBytes);
    const auto marker = appendLengthBearingMarker(destination, applicationSegment1MarkerCode, payload);
    return EncodedByteRange{
        marker.payloadRange.offsetBytes + standardXmpIdentifier.size() + 1,
        packetBytes.size(),
    };
}

struct AppendedExtendedXmpChunkReference final
{
    std::uint32_t completePacketLengthBytes;
    std::uint32_t chunkOffsetBytes;
    EncodedByteRange packetDataRange;
};

AppendedExtendedXmpChunkReference appendExtendedXmpChunk(std::vector<std::byte> &destination,
                                                         const std::string_view uppercaseMd5Guid,
                                                         const std::uint32_t completePacketLengthBytes,
                                                         const std::uint32_t chunkOffsetBytes,
                                                         const std::span<const std::uint8_t> packetData)
{
    constexpr std::string_view extendedXmpIdentifier = "http://ns.adobe.com/xmp/extension/";
    std::vector<std::uint8_t> payload;
    payload.reserve(extendedXmpIdentifier.size() + 1 + uppercaseMd5Guid.size() + 8 + packetData.size());
    for (const auto character : extendedXmpIdentifier)
    {
        payload.push_back(static_cast<std::uint8_t>(character));
    }
    payload.push_back(0);
    for (const auto character : uppercaseMd5Guid)
    {
        payload.push_back(static_cast<std::uint8_t>(character));
    }

    std::vector<std::byte> lengthAndOffsetBytes;
    appendBigEndianUnsigned32(lengthAndOffsetBytes, completePacketLengthBytes);
    appendBigEndianUnsigned32(lengthAndOffsetBytes, chunkOffsetBytes);
    for (const auto byte : lengthAndOffsetBytes)
    {
        payload.push_back(std::to_integer<std::uint8_t>(byte));
    }
    payload.insert(payload.end(), packetData.begin(), packetData.end());

    const auto marker = appendLengthBearingMarker(destination, applicationSegment1MarkerCode, payload);
    return {
        completePacketLengthBytes,
        chunkOffsetBytes,
        EncodedByteRange{
            marker.payloadRange.offsetBytes + extendedXmpIdentifier.size() + 1 + 32 + 8,
            packetData.size(),
        },
    };
}

AppendedExtendedXmpChunkReference appendExtendedXmpChunk(std::vector<std::byte> &destination,
                                                         const std::string_view uppercaseMd5Guid,
                                                         const std::uint32_t completePacketLengthBytes,
                                                         const std::uint32_t chunkOffsetBytes,
                                                         const std::initializer_list<std::uint8_t> packetData)
{
    return appendExtendedXmpChunk(destination, uppercaseMd5Guid, completePacketLengthBytes, chunkOffsetBytes,
                                  std::span(packetData.begin(), packetData.size()));
}

struct ExtendedXmpChunkFixture final
{
    std::string_view uppercaseMd5Guid;
    std::uint32_t completePacketLengthBytes;
    std::uint32_t chunkOffsetBytes;
    std::vector<std::uint8_t> packetData;
};

AppendedMarkerReference appendFrameHeader(std::vector<std::byte> &destination, const std::uint8_t markerCode,
                                          const std::uint8_t samplePrecisionBits, const std::uint8_t componentCount,
                                          const std::uint16_t widthPixels = 32, const std::uint16_t heightPixels = 16)
{
    std::vector<std::uint8_t> payload;
    payload.reserve(6 + (3 * componentCount));
    payload.push_back(samplePrecisionBits);
    payload.push_back(static_cast<std::uint8_t>(heightPixels >> 8));
    payload.push_back(static_cast<std::uint8_t>(heightPixels & 0xFF));
    payload.push_back(static_cast<std::uint8_t>(widthPixels >> 8));
    payload.push_back(static_cast<std::uint8_t>(widthPixels & 0xFF));
    payload.push_back(componentCount);
    for (std::uint16_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
        payload.push_back(static_cast<std::uint8_t>(componentIndex + 1));
        payload.push_back(0x11);
        payload.push_back(0);
    }
    return appendLengthBearingMarker(destination, markerCode, payload);
}

AppendedMarkerReference appendBaselineFrameHeader(std::vector<std::byte> &destination)
{
    return appendFrameHeader(destination, startOfFrameBaselineMarkerCode, 8, 1);
}

AppendedMarkerReference appendScanHeader(std::vector<std::byte> &destination, const std::uint8_t componentCount)
{
    std::vector<std::uint8_t> payload;
    payload.reserve(4 + (2 * componentCount));
    payload.push_back(componentCount);
    for (std::uint16_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
        payload.push_back(static_cast<std::uint8_t>(componentIndex + 1));
        payload.push_back(0);
    }
    payload.insert(payload.end(), {0, 63, 0});
    return appendLengthBearingMarker(destination, startOfScanMarkerCode, payload);
}

AppendedMarkerReference appendSingleComponentScanHeader(std::vector<std::byte> &destination)
{
    return appendScanHeader(destination, 1);
}

std::vector<std::byte> createBaselineJpegWithExtendedXmpChunks(
    const std::initializer_list<ExtendedXmpChunkFixture> chunks)
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    for (const auto &chunk : chunks)
    {
        appendExtendedXmpChunk(encodedJpeg, chunk.uppercaseMd5Guid, chunk.completePacketLengthBytes,
                               chunk.chunkOffsetBytes, chunk.packetData);
    }
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);
    return encodedJpeg;
}

std::vector<std::byte> createJpegWithFrame(const std::uint8_t frameMarkerCode, const std::uint8_t samplePrecisionBits,
                                           const std::uint8_t componentCount, const std::uint32_t scanCount = 1,
                                           const std::uint16_t widthPixels = 32, const std::uint16_t heightPixels = 16)
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendFrameHeader(encodedJpeg, frameMarkerCode, samplePrecisionBits, componentCount, widthPixels, heightPixels);
    for (std::uint32_t scanIndex = 0; scanIndex < scanCount; ++scanIndex)
    {
        appendScanHeader(encodedJpeg, componentCount);
        appendBytes(encodedJpeg, {0x42});
    }
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);
    return encodedJpeg;
}

std::vector<std::byte> createMinimalBaselineJpeg()
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);

    // 0xFF00 is one stuffed entropy byte. RST0 is a standalone marker inside
    // entropy-coded data and must not be mistaken for a length-bearing marker.
    appendBytes(encodedJpeg, {0x11, 0xFF, 0x00, 0x22});
    appendStandaloneMarker(encodedJpeg, restart0MarkerCode);
    appendBytes(encodedJpeg, {0x33});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);
    return encodedJpeg;
}

std::vector<std::byte> createBaselineJpegWithIccChunkHeaders(
    const std::initializer_list<std::pair<unsigned int, unsigned int>> chunkHeaders)
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    for (const auto &[sequenceNumber, chunkCount] : chunkHeaders)
    {
        appendIccProfileChunk(encodedJpeg, static_cast<std::uint8_t>(sequenceNumber),
                              static_cast<std::uint8_t>(chunkCount), {0x42});
    }
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);
    return encodedJpeg;
}

void requireMalformedJpeg(std::span<const std::byte> encodedJpeg)
{
    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedJpegStructure);
    REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::JpegStructureValidation);
}

void requireMalformedImageMetadata(std::span<const std::byte> encodedJpeg)
{
    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedImageMetadata);
    REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::JpegStructureValidation);
}
} // namespace

TEST_CASE("a minimal baseline JPEG produces checked immutable marker inventory", "[jpeg][scanner]")
{
    auto encodedJpeg = createMinimalBaselineJpeg();
    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.errorIfPresent() == nullptr);
    REQUIRE(result.valueIfPresent() != nullptr);

    const auto &inventory = *result.valueIfPresent();
    REQUIRE(inventory.encodedSourceLengthBytes() == encodedJpeg.size());
    REQUIRE(inventory.scanCount() == 1);
    REQUIRE(inventory.frameHeader().codingProcess == JpegCodingProcess::BaselineDctHuffman);
    REQUIRE(inventory.frameHeader().samplePrecisionBits == 8);
    REQUIRE(inventory.frameHeader().dimensions == ImageDimensions{PixelWidth{32}, PixelHeight{16}});
    REQUIRE(inventory.frameHeader().components.size() == 1);
    REQUIRE(inventory.frameHeader().components[0].componentIdentifier == 1);
    REQUIRE(inventory.frameHeader().components[0].horizontalSamplingFactor == 1);
    REQUIRE(inventory.frameHeader().components[0].verticalSamplingFactor == 1);
    REQUIRE(inventory.frameHeader().components[0].quantizationTableSelector == 0);
    REQUIRE(inventory.entropyCodedDataRanges().size() == 1);
    REQUIRE(inventory.endOfImageMarkerOffsetBytes() == encodedJpeg.size() - 2);

    for (const auto &marker : inventory.markers())
    {
        REQUIRE(marker.encodedRange.isContainedWithin(inventory.encodedSourceLengthBytes()));
        REQUIRE(marker.payloadRange.isContainedWithin(inventory.encodedSourceLengthBytes()));
    }
    for (const auto &entropyCodedDataRange : inventory.entropyCodedDataRanges())
    {
        REQUIRE(entropyCodedDataRange.isContainedWithin(inventory.encodedSourceLengthBytes()));
    }

    // Destroying the caller-owned byte sequence must not invalidate inventory;
    // it stores checked integer ranges and values, never spans or pointers.
    encodedJpeg.clear();
    encodedJpeg.shrink_to_fit();
    REQUIRE(inventory.frameHeader().dimensions.width.pixels == 32);
    REQUIRE(inventory.endOfImageMarkerOffsetBytes() == 32);
}

TEST_CASE("EOI separates the JPEG stream from every appended byte", "[jpeg][scanner][termination]")
{
    auto encodedJpeg = createMinimalBaselineJpeg();
    const auto jpegLength = static_cast<std::uint64_t>(encodedJpeg.size());
    SECTION("exact termination")
    {
    }
    SECTION("zero bytes are not silently assumed to be disposable padding")
    {
        appendBytes(encodedJpeg, {0, 0});
    }
    SECTION("appended content may itself contain marker-shaped bytes")
    {
        appendBytes(encodedJpeg, {0xFF, 0xD8, 0xFF, 0xD9, 0x42});
    }

    const auto result = JpegSegmentScanner::scan(encodedJpeg);
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto &inventory = *result.valueIfPresent();
    REQUIRE(inventory.endOfImageMarkerOffsetBytes() == jpegLength - 2);
    REQUIRE(inventory.trailingDataRange() == EncodedByteRange{jpegLength, encodedJpeg.size() - jpegLength});
    REQUIRE(inventory.trailingDataRange().isContainedWithin(inventory.encodedSourceLengthBytes()));
    REQUIRE(inventory.markers().back().markerCode == endOfImageMarkerCode);
}

TEST_CASE("APP and COM marker payloads retain exact source-relative ranges", "[jpeg][scanner]")
{
    std::vector<std::byte> encodedJpeg;
    std::vector<AppendedMarkerReference> expectedMarkers;
    expectedMarkers.push_back(appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode));
    expectedMarkers.push_back(
        appendLengthBearingMarker(encodedJpeg, applicationSegment1MarkerCode, {'E', 'x', 'i', 'f', 0, 0}));
    expectedMarkers.push_back(appendLengthBearingMarker(
        encodedJpeg, applicationSegment1MarkerCode,
        {'h', 't', 't', 'p', ':', '/', '/', 'n', 's', '.', 'a', 'd', 'o', 'b', 'e', '.', 'c', 'o', 'm', '/', 0}));
    expectedMarkers.push_back(
        appendLengthBearingMarker(encodedJpeg, applicationSegment2MarkerCode,
                                  {'I', 'C', 'C', '_', 'P', 'R', 'O', 'F', 'I', 'L', 'E', 0, 1, 1, 0x42}));
    expectedMarkers.push_back(
        appendLengthBearingMarker(encodedJpeg, applicationSegment2MarkerCode, {'o', 't', 'h', 'e', 'r'}));
    expectedMarkers.push_back(appendLengthBearingMarker(encodedJpeg, commentMarkerCode, {'o', 'k'}));
    expectedMarkers.push_back(appendBaselineFrameHeader(encodedJpeg));
    expectedMarkers.push_back(appendSingleComponentScanHeader(encodedJpeg));
    appendBytes(encodedJpeg, {0x42});
    expectedMarkers.push_back(appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode));

    const auto result = JpegSegmentScanner::scan(encodedJpeg);
    REQUIRE(result.valueIfPresent() != nullptr);
    REQUIRE(result.valueIfPresent()->markers().size() == expectedMarkers.size());

    for (std::size_t markerIndex = 0; markerIndex < expectedMarkers.size(); ++markerIndex)
    {
        CAPTURE(markerIndex);
        const auto &actualMarker = result.valueIfPresent()->markers()[markerIndex];
        const auto &expectedMarker = expectedMarkers[markerIndex];
        REQUIRE(actualMarker.markerCode == expectedMarker.markerCode);
        REQUIRE(actualMarker.encodedRange == expectedMarker.encodedRange);
        REQUIRE(actualMarker.payloadRange == expectedMarker.payloadRange);
    }
}

TEST_CASE("a complete ICC profile chunk set is inventoried in logical sequence order", "[jpeg][scanner][icc]")
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);

    // APP marker order is not semantic order. The ICC sequence fields and the
    // locked libjpeg-turbo public reader both permit this physical reordering.
    const auto secondChunk = appendIccProfileChunk(encodedJpeg, 2, 2, {0xB2, 0xB3});
    const auto firstChunk = appendIccProfileChunk(encodedJpeg, 1, 2, {0xA1});
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() != nullptr);
    const auto &iccProfileChunks = result.valueIfPresent()->iccProfileChunks();
    REQUIRE(iccProfileChunks.size() == 2);
    REQUIRE(iccProfileChunks[0].sequenceNumber == 1);
    REQUIRE(iccProfileChunks[0].chunkCount == 2);
    REQUIRE(iccProfileChunks[0].profileDataRange == firstChunk.profileDataRange);
    REQUIRE(iccProfileChunks[1].sequenceNumber == 2);
    REQUIRE(iccProfileChunks[1].chunkCount == 2);
    REQUIRE(iccProfileChunks[1].profileDataRange == secondChunk.profileDataRange);
}

TEST_CASE("Exif and standard XMP APP1 identifiers expose only their owned metadata bytes", "[jpeg][scanner][metadata]")
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    const auto expectedExifTiffDataRange = appendExifTiffData(encodedJpeg, {0x49, 0x49, 0x2A, 0x00});
    const auto expectedStandardXmpPacketRange = appendStandardXmpPacket(encodedJpeg, {'<', 'x', '/', '>'});
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() != nullptr);
    REQUIRE(result.valueIfPresent()->exifTiffDataRanges() == std::vector<EncodedByteRange>{expectedExifTiffDataRange});
    REQUIRE(result.valueIfPresent()->standardXmpPacketRanges() ==
            std::vector<EncodedByteRange>{expectedStandardXmpPacketRange});
}

TEST_CASE("reordered extended XMP chunks are inventoried by their Adobe-defined offsets", "[jpeg][scanner][xmp]")
{
    constexpr std::string_view uppercaseMd5Guid = "0123456789ABCDEF0123456789ABCDEF";
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendStandardXmpPacket(encodedJpeg, {'<', 'x', '/', '>'});

    const auto secondChunk = appendExtendedXmpChunk(encodedJpeg, uppercaseMd5Guid, 5, 2, {0xC2, 0xC3, 0xC4});
    const auto firstChunk = appendExtendedXmpChunk(encodedJpeg, uppercaseMd5Guid, 5, 0, {0xC0, 0xC1});
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() != nullptr);
    const auto &extendedXmpChunks = result.valueIfPresent()->extendedXmpChunks();
    REQUIRE(extendedXmpChunks.size() == 2);
    REQUIRE(std::string_view(extendedXmpChunks[0].packetGuid.uppercaseMd5HexadecimalCharacters.data(),
                             extendedXmpChunks[0].packetGuid.uppercaseMd5HexadecimalCharacters.size()) ==
            uppercaseMd5Guid);
    REQUIRE(extendedXmpChunks[0].completePacketLengthBytes == 5);
    REQUIRE(extendedXmpChunks[0].chunkOffsetBytes == 0);
    REQUIRE(extendedXmpChunks[0].packetDataRange == firstChunk.packetDataRange);
    REQUIRE(extendedXmpChunks[1].completePacketLengthBytes == 5);
    REQUIRE(extendedXmpChunks[1].chunkOffsetBytes == 2);
    REQUIRE(extendedXmpChunks[1].packetDataRange == secondChunk.packetDataRange);
}

TEST_CASE("ambiguous or incomplete extended XMP chunk envelopes fail closed", "[jpeg][scanner][xmp]")
{
    constexpr std::string_view firstGuid = "0123456789ABCDEF0123456789ABCDEF";
    constexpr std::string_view secondGuid = "FEDCBA9876543210FEDCBA9876543210";

    SECTION("lowercase hexadecimal GUID")
    {
        requireMalformedImageMetadata(
            createBaselineJpegWithExtendedXmpChunks({{"0123456789abcdef0123456789abcdef", 1, 0, {0x01}}}));
    }

    SECTION("non-hexadecimal GUID character")
    {
        requireMalformedImageMetadata(
            createBaselineJpegWithExtendedXmpChunks({{"G123456789ABCDEF0123456789ABCDEF", 1, 0, {0x01}}}));
    }

    SECTION("complete identifier without the fixed chunk header")
    {
        constexpr std::string_view extendedXmpIdentifier = "http://ns.adobe.com/xmp/extension/";
        std::vector<std::uint8_t> payload;
        for (const auto character : extendedXmpIdentifier)
        {
            payload.push_back(static_cast<std::uint8_t>(character));
        }
        payload.push_back(0);

        std::vector<std::byte> encodedJpeg;
        appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
        appendLengthBearingMarker(encodedJpeg, applicationSegment1MarkerCode, payload);
        appendBaselineFrameHeader(encodedJpeg);
        appendSingleComponentScanHeader(encodedJpeg);
        appendBytes(encodedJpeg, {0x42});
        appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

        requireMalformedImageMetadata(encodedJpeg);
    }

    SECTION("zero complete-packet length")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({{firstGuid, 0, 0, {}}}));
    }

    SECTION("missing final bytes")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({{firstGuid, 5, 0, {0x01, 0x02}}}));
    }

    SECTION("gap between chunks")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({
            {firstGuid, 5, 0, {0x01, 0x02}},
            {firstGuid, 5, 3, {0x04, 0x05}},
        }));
    }

    SECTION("overlapping chunks")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({
            {firstGuid, 4, 0, {0x01, 0x02, 0x03}},
            {firstGuid, 4, 2, {0x03, 0x04}},
        }));
    }

    SECTION("duplicate chunk offset")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({
            {firstGuid, 2, 0, {0x01}},
            {firstGuid, 2, 0, {0x02}},
        }));
    }

    SECTION("chunk range exceeds the declared complete length")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({{firstGuid, 4, 3, {0x04, 0x05}}}));
    }

    SECTION("chunks use different GUIDs")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({
            {firstGuid, 2, 0, {0x01}},
            {secondGuid, 2, 1, {0x02}},
        }));
    }

    SECTION("chunks use different complete lengths")
    {
        requireMalformedImageMetadata(createBaselineJpegWithExtendedXmpChunks({
            {firstGuid, 2, 0, {0x01}},
            {firstGuid, 3, 1, {0x02, 0x03}},
        }));
    }
}

TEST_CASE("an MP Format APP2 segment is rejected as a multi-picture JPEG", "[jpeg][scanner][mpf]")
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendLengthBearingMarker(encodedJpeg, applicationSegment2MarkerCode, {'M', 'P', 'F', 0, 'M', 'M', 0, 42});
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::MultiPictureJpegNotSupported);
    REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::JpegStructureValidation);
}

TEST_CASE("a fragmented C2PA manifest-store envelope is rejected before transformation", "[jpeg][scanner][jumbf]")
{
    const auto c2paManifestStore = createJumbfSuperbox(c2paManifestStoreTypeUuid, "c2pa");
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);

    // Splitting after twelve box bytes forces the scanner to reconstruct the
    // `jumd` UUID and label across APP11 segments instead of relying on a
    // fixed offset in the first physical marker.
    appendJpegXtBoxAsApp11Segments(encodedJpeg, c2paManifestStore, 0x0211, 12);
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);

    REQUIRE(result.valueIfPresent() == nullptr);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated);
    REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::JpegStructureValidation);
}

TEST_CASE("fragmented C2PA with a repeated extended box length is recognized", "[jpeg][scanner][jumbf]")
{
    auto box = createJumbfSuperbox(c2paManifestStoreTypeUuid, "c2pa");
    box[3] = 1;
    box.insert(box.begin() + 8, 8, 0);
    REQUIRE(box.size() < 256);
    box[15] = static_cast<std::uint8_t>(box.size());
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendJpegXtBoxAsApp11Segments(encodedJpeg, box, 1, 20);
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);
    const auto result = JpegSegmentScanner::scan(encodedJpeg);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated);

    // SOI(2) + first APP11(4 + 8 + 20) + second framing(4 + 8) +
    // LBox/TBox(8) reaches the repeated XLBox. Its last byte is offset 61.
    REQUIRE(encodedJpeg[61] == static_cast<std::byte>(box.size()));
    encodedJpeg[61] = static_cast<std::byte>(box.size() + 1);
    requireMalformedImageMetadata(encodedJpeg);
}

TEST_CASE("non-C2PA JUMBF is distinguished by exact UUID and label", "[jpeg][scanner][jumbf]")
{
    const auto requireUnsupportedJumbf = [](const std::array<std::uint8_t, 16> &typeUuid,
                                            const std::string_view label) {
        const auto jumbfSuperbox = createJumbfSuperbox(typeUuid, label);
        std::vector<std::byte> encodedJpeg;
        appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
        appendJpegXtBoxAsApp11Segments(encodedJpeg, jumbfSuperbox, 0x0037, jumbfSuperbox.size());
        appendBaselineFrameHeader(encodedJpeg);
        appendSingleComponentScanHeader(encodedJpeg);
        appendBytes(encodedJpeg, {0x42});
        appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

        const auto result = JpegSegmentScanner::scan(encodedJpeg);
        REQUIRE(result.valueIfPresent() == nullptr);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedJumbfMetadata);
    };

    SECTION("a different type and label")
    {
        requireUnsupportedJumbf(jsonContentTypeUuid, "metadata");
    }

    SECTION("the C2PA UUID alone is insufficient")
    {
        requireUnsupportedJumbf(c2paManifestStoreTypeUuid, "not-c2pa");
    }

    SECTION("the C2PA label alone is insufficient")
    {
        requireUnsupportedJumbf(jsonContentTypeUuid, "c2pa");
    }
}

TEST_CASE("ordinary APP11 metadata is not mistaken for a JPEG XT JUMBF envelope", "[jpeg][scanner][jumbf]")
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendLengthBearingMarker(encodedJpeg, applicationSegment11MarkerCode, {'J', 'X', 0x42, 0x43});
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);
    REQUIRE(result.valueIfPresent() != nullptr);
}

TEST_CASE("a non-JUMBF JPEG XT box is not conflated with JUMBF metadata", "[jpeg][scanner][jumbf]")
{
    auto jpegXtBox = createJumbfSuperbox(jsonContentTypeUuid, "metadata");
    jpegXtBox[4] = 'f';
    jpegXtBox[5] = 'r';
    jpegXtBox[6] = 'e';
    jpegXtBox[7] = 'e';

    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendJpegXtBoxAsApp11Segments(encodedJpeg, jpegXtBox, 0x0038, jpegXtBox.size());
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);
    REQUIRE(result.valueIfPresent() != nullptr);
}

TEST_CASE("ambiguous or incomplete ICC profile chunk numbering fails closed", "[jpeg][scanner][icc]")
{
    SECTION("missing sequence number")
    {
        requireMalformedImageMetadata(createBaselineJpegWithIccChunkHeaders({{1, 2}}));
    }

    SECTION("duplicate sequence number")
    {
        requireMalformedImageMetadata(createBaselineJpegWithIccChunkHeaders({{1, 2}, {1, 2}}));
    }

    SECTION("zero sequence number")
    {
        requireMalformedImageMetadata(createBaselineJpegWithIccChunkHeaders({{0, 1}}));
    }

    SECTION("sequence number exceeds chunk count")
    {
        requireMalformedImageMetadata(createBaselineJpegWithIccChunkHeaders({{2, 1}}));
    }

    SECTION("zero chunk count")
    {
        requireMalformedImageMetadata(createBaselineJpegWithIccChunkHeaders({{1, 0}}));
    }

    SECTION("inconsistent chunk counts")
    {
        requireMalformedImageMetadata(createBaselineJpegWithIccChunkHeaders({{1, 2}, {2, 3}, {3, 3}}));
    }

    SECTION("complete identifier without sequence and count fields")
    {
        std::vector<std::byte> encodedJpeg;
        appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
        appendLengthBearingMarker(encodedJpeg, applicationSegment2MarkerCode,
                                  {'I', 'C', 'C', '_', 'P', 'R', 'O', 'F', 'I', 'L', 'E', 0});
        appendBaselineFrameHeader(encodedJpeg);
        appendSingleComponentScanHeader(encodedJpeg);
        appendBytes(encodedJpeg, {0x42});
        appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

        requireMalformedImageMetadata(encodedJpeg);
    }

    SECTION("an ICC marker set containing no profile data")
    {
        std::vector<std::byte> encodedJpeg;
        appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
        appendIccProfileChunk(encodedJpeg, 1, 1, {});
        appendBaselineFrameHeader(encodedJpeg);
        appendSingleComponentScanHeader(encodedJpeg);
        appendBytes(encodedJpeg, {0x42});
        appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

        requireMalformedImageMetadata(encodedJpeg);
    }
}

TEST_CASE("every incomplete prefix of a length-bearing marker fails closed", "[jpeg][scanner]")
{
    struct MarkerTruncationCase final
    {
        std::string_view markerDescription;
        std::vector<std::byte> completePrefix;
        AppendedMarkerReference marker;
    };

    std::vector<MarkerTruncationCase> cases;
    {
        std::vector<std::byte> prefix;
        appendStandaloneMarker(prefix, startOfImageMarkerCode);
        const auto marker = appendLengthBearingMarker(prefix, applicationSegment1MarkerCode, {1, 2, 3, 4});
        cases.push_back({"APP1", std::move(prefix), marker});
    }
    {
        std::vector<std::byte> prefix;
        appendStandaloneMarker(prefix, startOfImageMarkerCode);
        const auto marker = appendBaselineFrameHeader(prefix);
        cases.push_back({"SOF0", std::move(prefix), marker});
    }
    {
        std::vector<std::byte> prefix;
        appendStandaloneMarker(prefix, startOfImageMarkerCode);
        appendBaselineFrameHeader(prefix);
        const auto marker = appendSingleComponentScanHeader(prefix);
        cases.push_back({"SOS", std::move(prefix), marker});
    }

    for (const auto &testCase : cases)
    {
        for (std::uint64_t retainedMarkerByteCount = 0;
             retainedMarkerByteCount < testCase.marker.encodedRange.lengthBytes; ++retainedMarkerByteCount)
        {
            DYNAMIC_SECTION(testCase.markerDescription << " retained marker bytes=" << retainedMarkerByteCount)
            {
                const auto retainedSourceLength =
                    static_cast<std::size_t>(testCase.marker.encodedRange.offsetBytes + retainedMarkerByteCount);
                requireMalformedJpeg(std::span(testCase.completePrefix).first(retainedSourceLength));
            }
        }
    }
}

TEST_CASE("impossible marker lengths are rejected before payload arithmetic", "[jpeg][scanner]")
{
    SECTION("declared length smaller than its own two-byte field")
    {
        const std::array encodedJpeg{
            std::byte{0xFF}, std::byte{startOfImageMarkerCode},
            std::byte{0xFF}, std::byte{applicationSegment1MarkerCode},
            std::byte{0x00}, std::byte{0x01},
        };
        requireMalformedJpeg(encodedJpeg);
    }

    SECTION("declared payload extends beyond the input")
    {
        const std::array encodedJpeg{
            std::byte{0xFF}, std::byte{startOfImageMarkerCode},
            std::byte{0xFF}, std::byte{applicationSegment1MarkerCode},
            std::byte{0x01}, std::byte{0x00},
            std::byte{0x42},
        };
        requireMalformedJpeg(encodedJpeg);
    }
}

TEST_CASE("encoded source length is bounded before marker parsing", "[jpeg][scanner]")
{
    const auto encodedJpeg = createMinimalBaselineJpeg();
    const JpegResourceLimits exactLengthLimits{
        encodedJpeg.size(),
        JpegResourceLimits::production().maximumPixelCount,
        JpegResourceLimits::production().maximumMetadataLengthBytes,
        JpegResourceLimits::production().maximumProgressiveScanCount,
    };

    SECTION("the exact encoded-length limit is accepted")
    {
        const auto result = JpegSegmentScanner::scan(encodedJpeg, exactLengthLimits);
        REQUIRE(result.valueIfPresent() != nullptr);
    }

    SECTION("one byte beyond the encoded-length limit is rejected with typed context")
    {
        const JpegResourceLimits oneByteSmallerLimit{
            encodedJpeg.size() - 1,
            exactLengthLimits.maximumPixelCount,
            exactLengthLimits.maximumMetadataLengthBytes,
            exactLengthLimits.maximumProgressiveScanCount,
        };
        const auto result = JpegSegmentScanner::scan(encodedJpeg, oneByteSmallerLimit);

        REQUIRE(result.valueIfPresent() == nullptr);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::EncodedFileTooLarge);
        const auto *const violation =
            std::get_if<JpegResourceLimitViolation>(&result.errorIfPresent()->diagnosticContext);
        REQUIRE(violation != nullptr);
        REQUIRE(violation->resourceLimit == JpegResourceLimit::EncodedFileLengthBytes);
        REQUIRE(violation->observedValue == encodedJpeg.size());
        REQUIRE(violation->maximumValue == encodedJpeg.size() - 1);
    }
}

TEST_CASE("supported JPEG coding processes retain their exact semantic identity", "[jpeg][scanner]")
{
    struct SupportedCodingProcessCase final
    {
        std::uint8_t frameMarkerCode;
        std::uint8_t samplePrecisionBits;
        JpegCodingProcess expectedCodingProcess;
    };

    constexpr std::array cases{
        SupportedCodingProcessCase{0xC0, 8, JpegCodingProcess::BaselineDctHuffman},
        SupportedCodingProcessCase{0xC1, 8, JpegCodingProcess::ExtendedSequentialDctHuffman},
        SupportedCodingProcessCase{0xC1, 12, JpegCodingProcess::ExtendedSequentialDctHuffman},
        SupportedCodingProcessCase{0xC2, 8, JpegCodingProcess::ProgressiveDctHuffman},
        SupportedCodingProcessCase{0xC2, 12, JpegCodingProcess::ProgressiveDctHuffman},
        SupportedCodingProcessCase{0xC9, 8, JpegCodingProcess::ExtendedSequentialDctArithmetic},
        SupportedCodingProcessCase{0xC9, 12, JpegCodingProcess::ExtendedSequentialDctArithmetic},
        SupportedCodingProcessCase{0xCA, 8, JpegCodingProcess::ProgressiveDctArithmetic},
        SupportedCodingProcessCase{0xCA, 12, JpegCodingProcess::ProgressiveDctArithmetic},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.frameMarkerCode, testCase.samplePrecisionBits);
        const auto encodedJpeg = createJpegWithFrame(testCase.frameMarkerCode, testCase.samplePrecisionBits, 1);
        const auto result = JpegSegmentScanner::scan(encodedJpeg);

        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(result.valueIfPresent()->frameHeader().codingProcess == testCase.expectedCodingProcess);
        REQUIRE(result.valueIfPresent()->frameHeader().samplePrecisionBits == testCase.samplePrecisionBits);
    }
}

TEST_CASE("predictive and differential JPEG processes are rejected as unsupported", "[jpeg][scanner]")
{
    constexpr std::array unsupportedFrameMarkerCodes{
        std::uint8_t{0xC3}, std::uint8_t{0xC5}, std::uint8_t{0xC6}, std::uint8_t{0xC7}, std::uint8_t{0xCB},
        std::uint8_t{0xCD}, std::uint8_t{0xCE}, std::uint8_t{0xCF}, std::uint8_t{0xF7}, // JPEG-LS SOF55 is outside the
                                                                                        // JPEG-1 transform contract.
    };

    for (const auto frameMarkerCode : unsupportedFrameMarkerCodes)
    {
        CAPTURE(frameMarkerCode);
        const auto result = JpegSegmentScanner::scan(createJpegWithFrame(frameMarkerCode, 8, 1));

        REQUIRE(result.valueIfPresent() == nullptr);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedJpegCodingProcess);
    }
}

TEST_CASE("sample precision follows the selected JPEG coding process", "[jpeg][scanner]")
{
    struct UnsupportedPrecisionCase final
    {
        std::uint8_t frameMarkerCode;
        std::uint8_t samplePrecisionBits;
    };

    constexpr std::array cases{
        UnsupportedPrecisionCase{0xC0, 12}, // Baseline DCT is exactly 8-bit.
        UnsupportedPrecisionCase{0xC1, 7},  UnsupportedPrecisionCase{0xC1, 9},  UnsupportedPrecisionCase{0xC2, 16},
        UnsupportedPrecisionCase{0xC9, 7},  UnsupportedPrecisionCase{0xCA, 16},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.frameMarkerCode, testCase.samplePrecisionBits);
        const auto result =
            JpegSegmentScanner::scan(createJpegWithFrame(testCase.frameMarkerCode, testCase.samplePrecisionBits, 1));

        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedJpegSamplePrecision);
    }
}

TEST_CASE("supported JPEG component organizations are limited to one, three, or four components", "[jpeg][scanner]")
{
    for (const std::uint8_t componentCount : {std::uint8_t{1}, std::uint8_t{3}, std::uint8_t{4}})
    {
        CAPTURE(componentCount);
        const auto result = JpegSegmentScanner::scan(createJpegWithFrame(0xC0, 8, componentCount));
        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(result.valueIfPresent()->frameHeader().components.size() == componentCount);
    }

    for (const std::uint8_t componentCount : {std::uint8_t{2}, std::uint8_t{5}})
    {
        CAPTURE(componentCount);
        const auto result = JpegSegmentScanner::scan(createJpegWithFrame(0xC0, 8, componentCount));
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedJpegComponentOrganization);
    }
}

TEST_CASE("SOF pixel count is checked at and one pixel beyond the injected bound", "[jpeg][scanner]")
{
    constexpr JpegResourceLimits limits{
        1024,
        100,
        1024,
        100,
    };

    SECTION("the exact pixel-count limit is accepted")
    {
        const auto result = JpegSegmentScanner::scan(createJpegWithFrame(0xC0, 8, 1, 1, 10, 10), limits);
        REQUIRE(result.valueIfPresent() != nullptr);
    }

    SECTION("one pixel beyond the limit is rejected with typed context")
    {
        const auto result = JpegSegmentScanner::scan(createJpegWithFrame(0xC0, 8, 1, 1, 101, 1), limits);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PixelCountLimitExceeded);
        const auto *const violation =
            std::get_if<JpegResourceLimitViolation>(&result.errorIfPresent()->diagnosticContext);
        REQUIRE(violation != nullptr);
        REQUIRE(violation->resourceLimit == JpegResourceLimit::PixelCount);
        REQUIRE(violation->observedValue == 101);
        REQUIRE(violation->maximumValue == 100);
    }
}

TEST_CASE("APP and COM encoded bytes share one aggregate metadata bound", "[jpeg][scanner]")
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    const auto metadataMarker = appendLengthBearingMarker(encodedJpeg, applicationSegment1MarkerCode, {1, 2, 3});
    appendBaselineFrameHeader(encodedJpeg);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const JpegResourceLimits exactMetadataLimits{
        1024,
        1024,
        metadataMarker.encodedRange.lengthBytes,
        100,
    };

    SECTION("the exact aggregate metadata limit is accepted and inventoried")
    {
        const auto result = JpegSegmentScanner::scan(encodedJpeg, exactMetadataLimits);
        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(result.valueIfPresent()->metadataEncodedLengthBytes() == metadataMarker.encodedRange.lengthBytes);
    }

    SECTION("one metadata byte beyond the limit is rejected with typed context")
    {
        const JpegResourceLimits oneByteSmallerLimit{
            exactMetadataLimits.maximumEncodedFileLengthBytes,
            exactMetadataLimits.maximumPixelCount,
            exactMetadataLimits.maximumMetadataLengthBytes - 1,
            exactMetadataLimits.maximumProgressiveScanCount,
        };
        const auto result = JpegSegmentScanner::scan(encodedJpeg, oneByteSmallerLimit);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::MetadataLengthLimitExceeded);
        const auto *const violation =
            std::get_if<JpegResourceLimitViolation>(&result.errorIfPresent()->diagnosticContext);
        REQUIRE(violation != nullptr);
        REQUIRE(violation->resourceLimit == JpegResourceLimit::MetadataLengthBytes);
        REQUIRE(violation->observedValue == metadataMarker.encodedRange.lengthBytes);
        REQUIRE(violation->maximumValue == metadataMarker.encodedRange.lengthBytes - 1);
    }
}

TEST_CASE("progressive scan count is accepted at 100 and rejected at 101", "[jpeg][scanner]")
{
    constexpr auto limits = JpegResourceLimits::production();

    const auto atLimitResult = JpegSegmentScanner::scan(createJpegWithFrame(0xC2, 8, 1, 100), limits);
    REQUIRE(atLimitResult.valueIfPresent() != nullptr);
    REQUIRE(atLimitResult.valueIfPresent()->scanCount() == 100);

    const auto overLimitResult = JpegSegmentScanner::scan(createJpegWithFrame(0xC2, 8, 1, 101), limits);
    REQUIRE(overLimitResult.errorIfPresent() != nullptr);
    REQUIRE(overLimitResult.errorIfPresent()->code == ImageProcessingErrorCode::ProgressiveScanCountLimitExceeded);
    const auto *const violation =
        std::get_if<JpegResourceLimitViolation>(&overLimitResult.errorIfPresent()->diagnosticContext);
    REQUIRE(violation != nullptr);
    REQUIRE(violation->resourceLimit == JpegResourceLimit::ProgressiveScanCount);
    REQUIRE(violation->observedValue == 101);
    REQUIRE(violation->maximumValue == 100);
}

TEST_CASE("marker syntax requires SOI first and EOI after entropy-coded data", "[jpeg][scanner]")
{
    SECTION("missing SOI")
    {
        const std::array encodedJpeg{std::byte{0xFF}, std::byte{endOfImageMarkerCode}};
        requireMalformedJpeg(encodedJpeg);
    }

    SECTION("missing EOI")
    {
        auto encodedJpeg = createMinimalBaselineJpeg();
        encodedJpeg.resize(encodedJpeg.size() - 2);
        requireMalformedJpeg(encodedJpeg);
    }

    SECTION("duplicate frame header")
    {
        std::vector<std::byte> encodedJpeg;
        appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
        appendBaselineFrameHeader(encodedJpeg);
        appendBaselineFrameHeader(encodedJpeg);
        appendSingleComponentScanHeader(encodedJpeg);
        appendBytes(encodedJpeg, {0x42});
        appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);
        requireMalformedJpeg(encodedJpeg);
    }
}

TEST_CASE("frame sampling and table selectors respect T81 table B2", "[jpeg][scanner][frame]")
{
    // SOI occupies two bytes; SOF's first component begins after its four-byte
    // framing and six-byte fixed payload. Change one field in an otherwise
    // structurally accepted input so the failure cannot come from truncation.
    for (const auto sampling : {0x01, 0x10, 0x51, 0x15, 0xFF})
    {
        auto encodedJpeg = createMinimalBaselineJpeg();
        encodedJpeg[13] = static_cast<std::byte>(sampling);
        requireMalformedJpeg(encodedJpeg);
    }
    auto encodedJpeg = createMinimalBaselineJpeg();
    encodedJpeg[14] = std::byte{4};
    requireMalformedJpeg(encodedJpeg);

    // Each nibble may reach four independently for a noninterleaved scan.
    encodedJpeg[13] = std::byte{0x44};
    encodedJpeg[14] = std::byte{3};
    REQUIRE(JpegSegmentScanner::scan(encodedJpeg).valueIfPresent() != nullptr);
}

TEST_CASE("hierarchical and reserved JPEG process markers cannot pass as opaque metadata", "[jpeg][scanner][process]")
{
    for (const auto markerCode : {0xC8, 0xDE, 0xDF})
    {
        auto encodedJpeg = createMinimalBaselineJpeg();
        // Insert a complete segment before SOF so the normal baseline frame
        // cannot hide a process-changing extension or hierarchical header.
        const std::array marker{std::byte{0xFF}, static_cast<std::byte>(markerCode), std::byte{0}, std::byte{2}};
        encodedJpeg.insert(encodedJpeg.begin() + 2, marker.begin(), marker.end());
        const auto result = JpegSegmentScanner::scan(encodedJpeg);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedJpegCodingProcess);
    }
}

TEST_CASE("deferred JPEG height is unsupported rather than classified as corrupt", "[jpeg][scanner][dnl]")
{
    std::vector<std::byte> encodedJpeg;
    appendStandaloneMarker(encodedJpeg, startOfImageMarkerCode);
    appendFrameHeader(encodedJpeg, startOfFrameBaselineMarkerCode, 8, 1, 32, 0);
    appendSingleComponentScanHeader(encodedJpeg);
    appendBytes(encodedJpeg, {0x42});
    // T.81 B.2.5: DNL follows the first scan and supplies the deferred height.
    appendLengthBearingMarker(encodedJpeg, 0xDC, {0, 16});
    appendStandaloneMarker(encodedJpeg, endOfImageMarkerCode);

    const auto result = JpegSegmentScanner::scan(encodedJpeg);
    REQUIRE(result.errorIfPresent() != nullptr);
    REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::UnsupportedJpegDeferredHeight);
}
