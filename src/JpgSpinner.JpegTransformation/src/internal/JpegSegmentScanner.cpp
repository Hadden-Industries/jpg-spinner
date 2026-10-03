#include "JpegSegmentScanner.h"

#include <jpg_spinner/domain/ImageProcessingError.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace jpg_spinner::jpeg::internal
{
namespace
{
using jpg_spinner::domain::ImageDimensions;
using jpg_spinner::domain::ImageProcessingError;
using jpg_spinner::domain::ImageProcessingErrorCode;
using jpg_spinner::domain::ImageProcessingResult;
using jpg_spinner::domain::ImageProcessingStage;
using jpg_spinner::domain::JpegCodingProcess;
using jpg_spinner::domain::JpegComponentDescription;
using jpg_spinner::domain::JpegResourceLimit;
using jpg_spinner::domain::JpegResourceLimitViolation;
using jpg_spinner::domain::PixelHeight;
using jpg_spinner::domain::PixelWidth;

constexpr std::uint8_t markerPrefix = 0xFF;
constexpr std::uint8_t stuffedEntropyByteCode = 0x00;
constexpr std::uint8_t temporaryMarkerCode = 0x01;
constexpr std::uint8_t startOfFrameBaselineMarkerCode = 0xC0;
constexpr std::uint8_t startOfFrameExtendedSequentialHuffmanMarkerCode = 0xC1;
constexpr std::uint8_t startOfFrameProgressiveHuffmanMarkerCode = 0xC2;
constexpr std::uint8_t startOfFrameLosslessHuffmanMarkerCode = 0xC3;
constexpr std::uint8_t startOfFrameDifferentialSequentialHuffmanMarkerCode = 0xC5;
constexpr std::uint8_t startOfFrameDifferentialProgressiveHuffmanMarkerCode = 0xC6;
constexpr std::uint8_t startOfFrameDifferentialLosslessHuffmanMarkerCode = 0xC7;
constexpr std::uint8_t reservedJpegExtensionMarkerCode = 0xC8;
constexpr std::uint8_t startOfFrameExtendedSequentialArithmeticMarkerCode = 0xC9;
constexpr std::uint8_t startOfFrameProgressiveArithmeticMarkerCode = 0xCA;
constexpr std::uint8_t startOfFrameLosslessArithmeticMarkerCode = 0xCB;
constexpr std::uint8_t startOfFrameDifferentialSequentialArithmeticMarkerCode = 0xCD;
constexpr std::uint8_t startOfFrameDifferentialProgressiveArithmeticMarkerCode = 0xCE;
constexpr std::uint8_t startOfFrameDifferentialLosslessArithmeticMarkerCode = 0xCF;
constexpr std::uint8_t restart0MarkerCode = 0xD0;
constexpr std::uint8_t restart7MarkerCode = 0xD7;
constexpr std::uint8_t startOfImageMarkerCode = 0xD8;
constexpr std::uint8_t endOfImageMarkerCode = 0xD9;
constexpr std::uint8_t startOfScanMarkerCode = 0xDA;
constexpr std::uint8_t defineHierarchicalProgressionMarkerCode = 0xDE;
constexpr std::uint8_t expandReferenceComponentsMarkerCode = 0xDF;
constexpr std::uint8_t applicationSegment0MarkerCode = 0xE0;
constexpr std::uint8_t applicationSegment1MarkerCode = 0xE1;
constexpr std::uint8_t applicationSegment2MarkerCode = 0xE2;
constexpr std::uint8_t applicationSegment11MarkerCode = 0xEB;
constexpr std::uint8_t applicationSegment15MarkerCode = 0xEF;
constexpr std::uint8_t commentMarkerCode = 0xFE;
constexpr std::uint8_t jpegLsStartOfFrameMarkerCode = 0xF7;
constexpr std::array<std::uint8_t, 6> exifIdentifier{'E', 'x', 'i', 'f', 0, 0};
constexpr std::array<std::uint8_t, 2> jpegXtBoxFragmentIdentifier{'J', 'P'};
constexpr std::array<std::uint8_t, 4> jumbfSuperboxType{'j', 'u', 'm', 'b'};
constexpr std::array<std::uint8_t, 4> jumbfDescriptionBoxType{'j', 'u', 'm', 'd'};
constexpr std::array<std::uint8_t, 16> c2paManifestStoreTypeUuid{
    0x63, 0x32, 0x70, 0x61, 0x00, 0x11, 0x00, 0x10, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
};
constexpr std::array<std::uint8_t, 5> c2paManifestStoreLabel{'c', '2', 'p', 'a', 0};
constexpr std::uint64_t jpegXtFragmentEnvelopeLengthBytes = 8;
constexpr std::uint64_t basicIsoBoxHeaderLengthBytes = 8;
constexpr std::uint64_t largeIsoBoxHeaderLengthBytes = 16;
constexpr std::size_t maximumJumbfIdentificationPrefixLengthBytes = 64;

template <std::size_t CharacterCount>
[[nodiscard]] consteval std::array<std::uint8_t, CharacterCount> nullTerminatedAsciiIdentifier(
    const char (&characters)[CharacterCount]) noexcept
{
    std::array<std::uint8_t, CharacterCount> identifier{};
    for (std::size_t characterIndex = 0; characterIndex < CharacterCount; ++characterIndex)
    {
        identifier[characterIndex] = static_cast<std::uint8_t>(characters[characterIndex]);
    }
    return identifier;
}

constexpr auto standardXmpIdentifier = nullTerminatedAsciiIdentifier("http://ns.adobe.com/xap/1.0/");
constexpr auto extendedXmpIdentifier = nullTerminatedAsciiIdentifier("http://ns.adobe.com/xmp/extension/");
constexpr auto iccProfileIdentifier = nullTerminatedAsciiIdentifier("ICC_PROFILE");
constexpr auto mpFormatIdentifier = nullTerminatedAsciiIdentifier("MPF");
constexpr std::uint64_t iccProfileChunkHeaderLengthBytes = iccProfileIdentifier.size() + 2;
constexpr std::uint64_t extendedXmpGuidLengthBytes = 32;
constexpr std::uint64_t extendedXmpChunkHeaderLengthBytes =
    extendedXmpIdentifier.size() + extendedXmpGuidLengthBytes + 4 + 4;

[[nodiscard]] std::uint8_t byteValue(const std::byte value) noexcept
{
    return std::to_integer<std::uint8_t>(value);
}

/// Reads one JPEG two-byte unsigned integer only after proving both bytes are
/// contained. Every marker length and 16-bit frame dimension passes through
/// this function so no call site can accidentally perform an unchecked read.
[[nodiscard]] std::optional<std::uint16_t> tryReadBigEndianUnsigned16(const std::span<const std::byte> encodedJpeg,
                                                                      const std::size_t offsetBytes) noexcept
{
    if (offsetBytes > encodedJpeg.size() || encodedJpeg.size() - offsetBytes < 2)
    {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(byteValue(encodedJpeg[offsetBytes])) << 8) |
                                      byteValue(encodedJpeg[offsetBytes + 1]));
}

[[nodiscard]] std::optional<std::uint32_t> tryReadBigEndianUnsigned32(const std::span<const std::byte> encodedJpeg,
                                                                      const std::size_t offsetBytes) noexcept
{
    if (offsetBytes > encodedJpeg.size() || encodedJpeg.size() - offsetBytes < 4)
    {
        return std::nullopt;
    }

    return (static_cast<std::uint32_t>(byteValue(encodedJpeg[offsetBytes])) << 24) |
           (static_cast<std::uint32_t>(byteValue(encodedJpeg[offsetBytes + 1])) << 16) |
           (static_cast<std::uint32_t>(byteValue(encodedJpeg[offsetBytes + 2])) << 8) |
           byteValue(encodedJpeg[offsetBytes + 3]);
}

[[nodiscard]] std::optional<std::uint64_t> tryReadBigEndianUnsigned64(const std::span<const std::byte> encodedJpeg,
                                                                      const std::size_t offsetBytes) noexcept
{
    if (offsetBytes > encodedJpeg.size() || encodedJpeg.size() - offsetBytes < 8)
    {
        return std::nullopt;
    }

    std::uint64_t value = 0;
    for (std::size_t byteIndex = 0; byteIndex < 8; ++byteIndex)
    {
        value = (value << 8) | byteValue(encodedJpeg[offsetBytes + byteIndex]);
    }
    return value;
}

[[nodiscard]] ImageProcessingResult<JpegMarkerInventory> malformedJpegStructure() noexcept
{
    return ImageProcessingResult<JpegMarkerInventory>::failure(ImageProcessingError{
        ImageProcessingErrorCode::MalformedJpegStructure,
        ImageProcessingStage::JpegStructureValidation,
    });
}

[[nodiscard]] ImageProcessingResult<JpegMarkerInventory> encodedFileTooLarge(
    const std::uint64_t observedLengthBytes, const std::uint64_t maximumLengthBytes) noexcept
{
    return ImageProcessingResult<JpegMarkerInventory>::failure(ImageProcessingError{
        ImageProcessingErrorCode::EncodedFileTooLarge,
        ImageProcessingStage::JpegStructureValidation,
        {},
        JpegResourceLimitViolation{
            JpegResourceLimit::EncodedFileLengthBytes,
            observedLengthBytes,
            maximumLengthBytes,
        },
    });
}

[[nodiscard]] ImageProcessingResult<JpegMarkerInventory> scannerError(const ImageProcessingErrorCode errorCode) noexcept
{
    return ImageProcessingResult<JpegMarkerInventory>::failure(ImageProcessingError{
        errorCode,
        ImageProcessingStage::JpegStructureValidation,
    });
}

[[nodiscard]] ImageProcessingResult<JpegMarkerInventory> scannerResourceLimitError(
    const ImageProcessingErrorCode errorCode, const JpegResourceLimit resourceLimit, const std::uint64_t observedValue,
    const std::uint64_t maximumValue) noexcept
{
    return ImageProcessingResult<JpegMarkerInventory>::failure(ImageProcessingError{
        errorCode,
        ImageProcessingStage::JpegStructureValidation,
        {},
        JpegResourceLimitViolation{resourceLimit, observedValue, maximumValue},
    });
}

[[nodiscard]] constexpr bool isRestartMarker(const std::uint8_t markerCode) noexcept
{
    return markerCode >= restart0MarkerCode && markerCode <= restart7MarkerCode;
}

[[nodiscard]] constexpr bool isMetadataMarker(const std::uint8_t markerCode) noexcept
{
    return (markerCode >= applicationSegment0MarkerCode && markerCode <= applicationSegment15MarkerCode) ||
           markerCode == commentMarkerCode;
}

template <std::size_t IdentifierLengthBytes>
[[nodiscard]] bool markerPayloadStartsWith(const std::span<const std::byte> encodedJpeg,
                                           const JpegMarkerReference &marker, const std::uint8_t requiredMarkerCode,
                                           const std::array<std::uint8_t, IdentifierLengthBytes> &identifier) noexcept
{
    if (marker.markerCode != requiredMarkerCode || marker.payloadRange.lengthBytes < identifier.size())
    {
        return false;
    }

    const auto payloadOffset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
    return std::equal(
        identifier.begin(), identifier.end(), encodedJpeg.begin() + payloadOffset,
        [](const std::uint8_t expected, const std::byte actual) { return expected == byteValue(actual); });
}

template <std::size_t ExpectedLengthBytes>
[[nodiscard]] bool encodedBytesEqual(const std::span<const std::byte> encodedBytes, const std::size_t offsetBytes,
                                     const std::array<std::uint8_t, ExpectedLengthBytes> &expectedBytes) noexcept
{
    if (offsetBytes > encodedBytes.size() || expectedBytes.size() > encodedBytes.size() - offsetBytes)
    {
        return false;
    }

    return std::equal(
        expectedBytes.begin(), expectedBytes.end(), encodedBytes.begin() + offsetBytes,
        [](const std::uint8_t expected, const std::byte actual) { return expected == byteValue(actual); });
}

enum class JumbfEnvelopeClassification
{
    Absent,
    UnsupportedJumbf,
    C2paManifestStore,
    Malformed,
};

struct ActiveJpegXtBox final
{
    std::uint16_t instanceNumber;
    std::uint32_t nextPacketSequenceNumber;
    std::uint64_t declaredBoxLengthBytes;
    std::uint64_t reconstructedBoxLengthBytes;
    std::array<std::byte, 8> repeatedBoxHeader;
    std::vector<std::byte> identificationPrefix;
};

[[nodiscard]] bool hasC2paManifestStoreIdentity(const ActiveJpegXtBox &jpegXtBox) noexcept
{
    const std::span<const std::byte> prefix{jpegXtBox.identificationPrefix};
    const auto outerBoxLength32 = tryReadBigEndianUnsigned32(prefix, 0);
    if (!outerBoxLength32.has_value() || !encodedBytesEqual(prefix, 4, jumbfSuperboxType))
    {
        return false;
    }

    std::uint64_t outerHeaderLengthBytes = basicIsoBoxHeaderLengthBytes;
    if (*outerBoxLength32 == 1)
    {
        const auto largeOuterBoxLength = tryReadBigEndianUnsigned64(prefix, 8);
        if (!largeOuterBoxLength.has_value() || *largeOuterBoxLength != jpegXtBox.declaredBoxLengthBytes)
        {
            return false;
        }
        outerHeaderLengthBytes = largeIsoBoxHeaderLengthBytes;
    }
    else if (*outerBoxLength32 != jpegXtBox.declaredBoxLengthBytes)
    {
        return false;
    }

    const auto descriptionBoxOffset = static_cast<std::size_t>(outerHeaderLengthBytes);
    const auto descriptionBoxLength32 = tryReadBigEndianUnsigned32(prefix, descriptionBoxOffset);
    if (!descriptionBoxLength32.has_value() ||
        !encodedBytesEqual(prefix, descriptionBoxOffset + 4, jumbfDescriptionBoxType))
    {
        return false;
    }

    std::uint64_t descriptionBoxHeaderLengthBytes = basicIsoBoxHeaderLengthBytes;
    std::uint64_t descriptionBoxLengthBytes = *descriptionBoxLength32;
    if (*descriptionBoxLength32 == 1)
    {
        const auto largeDescriptionBoxLength = tryReadBigEndianUnsigned64(prefix, descriptionBoxOffset + 8);
        if (!largeDescriptionBoxLength.has_value())
        {
            return false;
        }
        descriptionBoxHeaderLengthBytes = largeIsoBoxHeaderLengthBytes;
        descriptionBoxLengthBytes = *largeDescriptionBoxLength;
    }

    const auto minimumDescriptionPayloadLengthBytes =
        c2paManifestStoreTypeUuid.size() + 1 + c2paManifestStoreLabel.size();
    if (descriptionBoxLengthBytes < descriptionBoxHeaderLengthBytes + minimumDescriptionPayloadLengthBytes ||
        descriptionBoxLengthBytes > jpegXtBox.declaredBoxLengthBytes - outerHeaderLengthBytes)
    {
        return false;
    }

    const auto descriptionPayloadOffset =
        descriptionBoxOffset + static_cast<std::size_t>(descriptionBoxHeaderLengthBytes);
    const auto togglesOffset = descriptionPayloadOffset + c2paManifestStoreTypeUuid.size();
    const auto labelOffset = togglesOffset + 1;
    if (!encodedBytesEqual(prefix, descriptionPayloadOffset, c2paManifestStoreTypeUuid) ||
        togglesOffset >= prefix.size() || (byteValue(prefix[togglesOffset]) & 0x03) != 0x03 ||
        !encodedBytesEqual(prefix, labelOffset, c2paManifestStoreLabel))
    {
        return false;
    }

    // C2PA 2.4 identifies the manifest store by this exact UUID-and-label
    // conjunction. The label comparison includes its null terminator, so a
    // longer look-alike such as "c2pa-old" cannot be classified as C2PA.
    return true;
}

[[nodiscard]] JumbfEnvelopeClassification classifyJumbfEnvelope(const std::span<const std::byte> encodedJpeg,
                                                                const std::vector<JpegMarkerReference> &markers)
{
    std::optional<ActiveJpegXtBox> activeBox;
    std::vector<std::uint16_t> observedInstanceNumbers;
    bool foundJumbf = false;
    bool foundC2paManifestStore = false;

    const auto finishCompleteBox = [&]() -> bool {
        if (!activeBox.has_value() || activeBox->reconstructedBoxLengthBytes != activeBox->declaredBoxLengthBytes)
        {
            return false;
        }

        foundJumbf = true;
        foundC2paManifestStore = foundC2paManifestStore || hasC2paManifestStoreIdentity(*activeBox);
        activeBox.reset();
        return true;
    };

    for (const auto &marker : markers)
    {
        const auto isJpegXtBoxFragment =
            markerPayloadStartsWith(encodedJpeg, marker, applicationSegment11MarkerCode, jpegXtBoxFragmentIdentifier);
        if (!isJpegXtBoxFragment)
        {
            // C2PA requires contiguous fragments. This bounded recognizer
            // does not support interrupted JUMBF transport; never preserve
            // an incomplete recognized box as generic APP11 metadata.
            if (activeBox.has_value())
            {
                return JumbfEnvelopeClassification::Malformed;
            }
            continue;
        }

        if (marker.payloadRange.lengthBytes < jpegXtFragmentEnvelopeLengthBytes + basicIsoBoxHeaderLengthBytes)
        {
            // `JP` identifies the broader JPEG XT box transport, not JUMBF by
            // itself. Without a complete repeated LBox/TBox, an isolated APP11
            // remains opaque metadata; inside an active `jumb` sequence it is
            // an invalid interruption and must fail closed.
            if (activeBox.has_value())
            {
                return JumbfEnvelopeClassification::Malformed;
            }
            continue;
        }

        const auto payloadOffset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
        const auto instanceNumber = tryReadBigEndianUnsigned16(encodedJpeg, payloadOffset + 2);
        const auto packetSequenceNumber = tryReadBigEndianUnsigned32(encodedJpeg, payloadOffset + 4);
        const auto boxLength32 = tryReadBigEndianUnsigned32(
            encodedJpeg, payloadOffset + static_cast<std::size_t>(jpegXtFragmentEnvelopeLengthBytes));
        if (!instanceNumber.has_value() || !packetSequenceNumber.has_value() || !boxLength32.has_value())
        {
            return JumbfEnvelopeClassification::Malformed;
        }

        std::array<std::byte, 8> repeatedBoxHeader{};
        std::copy_n(encodedJpeg.begin() + payloadOffset + static_cast<std::size_t>(jpegXtFragmentEnvelopeLengthBytes),
                    repeatedBoxHeader.size(), repeatedBoxHeader.begin());

        const auto isJumbfSuperbox = std::equal(
            jumbfSuperboxType.begin(), jumbfSuperboxType.end(), repeatedBoxHeader.begin() + 4,
            [](const std::uint8_t expected, const std::byte actual) { return expected == byteValue(actual); });
        if (!isJumbfSuperbox)
        {
            if (activeBox.has_value())
            {
                return JumbfEnvelopeClassification::Malformed;
            }
            continue;
        }

        std::size_t fragmentDataOffset = payloadOffset + static_cast<std::size_t>(jpegXtFragmentEnvelopeLengthBytes);
        std::uint64_t fragmentContributionLengthBytes =
            marker.payloadRange.lengthBytes - jpegXtFragmentEnvelopeLengthBytes;

        if (*packetSequenceNumber == 1)
        {
            if (activeBox.has_value() ||
                std::ranges::find(observedInstanceNumbers, *instanceNumber) != observedInstanceNumbers.end())
            {
                return JumbfEnvelopeClassification::Malformed;
            }

            std::uint64_t declaredBoxLengthBytes = *boxLength32;
            if (*boxLength32 == 1)
            {
                if (marker.payloadRange.lengthBytes < jpegXtFragmentEnvelopeLengthBytes + largeIsoBoxHeaderLengthBytes)
                {
                    return JumbfEnvelopeClassification::Malformed;
                }
                const auto largeBoxLength = tryReadBigEndianUnsigned64(
                    encodedJpeg, fragmentDataOffset + static_cast<std::size_t>(basicIsoBoxHeaderLengthBytes));
                if (!largeBoxLength.has_value())
                {
                    return JumbfEnvelopeClassification::Malformed;
                }
                declaredBoxLengthBytes = *largeBoxLength;
            }

            const auto requiredHeaderLengthBytes =
                *boxLength32 == 1 ? largeIsoBoxHeaderLengthBytes : basicIsoBoxHeaderLengthBytes;
            if (declaredBoxLengthBytes < requiredHeaderLengthBytes)
            {
                return JumbfEnvelopeClassification::Malformed;
            }

            observedInstanceNumbers.push_back(*instanceNumber);
            activeBox = ActiveJpegXtBox{
                *instanceNumber, 2, declaredBoxLengthBytes, 0, repeatedBoxHeader, {},
            };
            activeBox->identificationPrefix.reserve(maximumJumbfIdentificationPrefixLengthBytes);
        }
        else
        {
            const auto repeatedHeaderLengthBytes =
                *boxLength32 == 1 ? largeIsoBoxHeaderLengthBytes : basicIsoBoxHeaderLengthBytes;
            if (!activeBox.has_value() || *instanceNumber != activeBox->instanceNumber ||
                *packetSequenceNumber != activeBox->nextPacketSequenceNumber ||
                repeatedBoxHeader != activeBox->repeatedBoxHeader ||
                marker.payloadRange.lengthBytes <= jpegXtFragmentEnvelopeLengthBytes + repeatedHeaderLengthBytes)
            {
                return JumbfEnvelopeClassification::Malformed;
            }

            if (*boxLength32 == 1 &&
                tryReadBigEndianUnsigned64(encodedJpeg, fragmentDataOffset + 8) != activeBox->declaredBoxLengthBytes)
            {
                return JumbfEnvelopeClassification::Malformed;
            }
            // JPEG XT repeats LBox/TBox and optional XLBox in each packet,
            // as emitted by ISO/IEC 19566-10's DoubleBench reference writer.
            // Repeated framing does not contribute to reconstructed length.
            fragmentDataOffset += static_cast<std::size_t>(repeatedHeaderLengthBytes);
            fragmentContributionLengthBytes -= repeatedHeaderLengthBytes;
        }

        if (fragmentContributionLengthBytes >
            activeBox->declaredBoxLengthBytes - activeBox->reconstructedBoxLengthBytes)
        {
            return JumbfEnvelopeClassification::Malformed;
        }

        const auto remainingPrefixCapacity =
            maximumJumbfIdentificationPrefixLengthBytes - activeBox->identificationPrefix.size();
        const auto prefixByteCount =
            static_cast<std::size_t>(std::min<std::uint64_t>(fragmentContributionLengthBytes, remainingPrefixCapacity));
        activeBox->identificationPrefix.insert(activeBox->identificationPrefix.end(),
                                               encodedJpeg.begin() + fragmentDataOffset,
                                               encodedJpeg.begin() + fragmentDataOffset + prefixByteCount);
        activeBox->reconstructedBoxLengthBytes += fragmentContributionLengthBytes;

        if (activeBox->reconstructedBoxLengthBytes == activeBox->declaredBoxLengthBytes)
        {
            if (!finishCompleteBox())
            {
                return JumbfEnvelopeClassification::Malformed;
            }
        }
        else
        {
            if (*packetSequenceNumber == std::numeric_limits<std::uint32_t>::max())
            {
                return JumbfEnvelopeClassification::Malformed;
            }
            activeBox->nextPacketSequenceNumber = *packetSequenceNumber + 1;
        }
    }

    if (activeBox.has_value())
    {
        return JumbfEnvelopeClassification::Malformed;
    }
    if (foundC2paManifestStore)
    {
        return JumbfEnvelopeClassification::C2paManifestStore;
    }
    return foundJumbf ? JumbfEnvelopeClassification::UnsupportedJumbf : JumbfEnvelopeClassification::Absent;
}

[[nodiscard]] bool hasIccProfileIdentifier(const std::span<const std::byte> encodedJpeg,
                                           const JpegMarkerReference &marker) noexcept
{
    return markerPayloadStartsWith(encodedJpeg, marker, applicationSegment2MarkerCode, iccProfileIdentifier);
}

/// Validates only the ICC APP2 fragment envelope defined by ICC Technical
/// Note 10-2021. The profile body remains opaque and is later interpreted by
/// an ICC-aware library; this boundary merely proves one unambiguous complete
/// sequence without copying or parsing color-profile data.
[[nodiscard]] bool validateAndOrderIccProfileChunks(std::vector<IccProfileChunkReference> &iccProfileChunks) noexcept
{
    if (iccProfileChunks.empty())
    {
        return true;
    }

    const auto expectedChunkCount = iccProfileChunks.front().chunkCount;
    if (expectedChunkCount == 0 || iccProfileChunks.size() != expectedChunkCount)
    {
        return false;
    }

    std::array<bool, 256> observedSequenceNumbers{};
    std::uint64_t assembledProfileLengthBytes = 0;
    for (const auto &chunk : iccProfileChunks)
    {
        if (chunk.chunkCount != expectedChunkCount || chunk.sequenceNumber == 0 ||
            chunk.sequenceNumber > expectedChunkCount || observedSequenceNumbers[chunk.sequenceNumber])
        {
            return false;
        }

        observedSequenceNumbers[chunk.sequenceNumber] = true;
        assembledProfileLengthBytes += chunk.profileDataRange.lengthBytes;
    }
    if (assembledProfileLengthBytes == 0)
    {
        return false;
    }

    std::ranges::sort(iccProfileChunks, {}, &IccProfileChunkReference::sequenceNumber);
    return true;
}

[[nodiscard]] constexpr bool isUppercaseHexadecimalCharacter(const char character) noexcept
{
    return (character >= '0' && character <= '9') || (character >= 'A' && character <= 'F');
}

[[nodiscard]] bool isValidExtendedXmpGuid(const ExtendedXmpGuid &packetGuid) noexcept
{
    return std::ranges::all_of(packetGuid.uppercaseMd5HexadecimalCharacters, isUppercaseHexadecimalCharacter);
}

/// Adobe defines physical APP1 order as non-authoritative and requires robust
/// readers to reconstruct Extended XMP by its 32-bit offsets. Sorting first,
/// then requiring exact contiguous coverage, rejects gaps, overlap, duplicate
/// offsets, and ambiguous mixed serializations without inspecting RDF bytes.
[[nodiscard]] bool validateAndOrderExtendedXmpChunks(std::vector<ExtendedXmpChunkReference> &extendedXmpChunks) noexcept
{
    if (extendedXmpChunks.empty())
    {
        return true;
    }

    const auto expectedPacketGuid = extendedXmpChunks.front().packetGuid;
    const auto expectedCompletePacketLengthBytes = extendedXmpChunks.front().completePacketLengthBytes;
    if (!isValidExtendedXmpGuid(expectedPacketGuid) || expectedCompletePacketLengthBytes == 0)
    {
        return false;
    }

    for (const auto &chunk : extendedXmpChunks)
    {
        if (chunk.packetGuid != expectedPacketGuid || !isValidExtendedXmpGuid(chunk.packetGuid) ||
            chunk.completePacketLengthBytes != expectedCompletePacketLengthBytes ||
            chunk.chunkOffsetBytes > expectedCompletePacketLengthBytes ||
            chunk.packetDataRange.lengthBytes > expectedCompletePacketLengthBytes - chunk.chunkOffsetBytes)
        {
            return false;
        }
    }

    std::ranges::sort(extendedXmpChunks, {}, &ExtendedXmpChunkReference::chunkOffsetBytes);
    std::uint64_t nextExpectedChunkOffsetBytes = 0;
    for (const auto &chunk : extendedXmpChunks)
    {
        if (chunk.chunkOffsetBytes != nextExpectedChunkOffsetBytes)
        {
            return false;
        }
        nextExpectedChunkOffsetBytes += chunk.packetDataRange.lengthBytes;
    }

    return nextExpectedChunkOffsetBytes == expectedCompletePacketLengthBytes;
}

[[nodiscard]] constexpr std::optional<JpegCodingProcess> supportedCodingProcessFor(
    const std::uint8_t markerCode) noexcept
{
    switch (markerCode)
    {
    case startOfFrameBaselineMarkerCode:
        return JpegCodingProcess::BaselineDctHuffman;
    case startOfFrameExtendedSequentialHuffmanMarkerCode:
        return JpegCodingProcess::ExtendedSequentialDctHuffman;
    case startOfFrameProgressiveHuffmanMarkerCode:
        return JpegCodingProcess::ProgressiveDctHuffman;
    case startOfFrameExtendedSequentialArithmeticMarkerCode:
        return JpegCodingProcess::ExtendedSequentialDctArithmetic;
    case startOfFrameProgressiveArithmeticMarkerCode:
        return JpegCodingProcess::ProgressiveDctArithmetic;
    default:
        return std::nullopt;
    }
}

[[nodiscard]] constexpr bool isUnsupportedCodingProcessMarker(const std::uint8_t markerCode) noexcept
{
    switch (markerCode)
    {
    case startOfFrameLosslessHuffmanMarkerCode:
    case startOfFrameDifferentialSequentialHuffmanMarkerCode:
    case startOfFrameDifferentialProgressiveHuffmanMarkerCode:
    case startOfFrameDifferentialLosslessHuffmanMarkerCode:
    case startOfFrameLosslessArithmeticMarkerCode:
    case startOfFrameDifferentialSequentialArithmeticMarkerCode:
    case startOfFrameDifferentialProgressiveArithmeticMarkerCode:
    case startOfFrameDifferentialLosslessArithmeticMarkerCode:
    case jpegLsStartOfFrameMarkerCode:
    case reservedJpegExtensionMarkerCode:
    case defineHierarchicalProgressionMarkerCode:
    case expandReferenceComponentsMarkerCode:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool isProgressiveCodingProcess(const JpegCodingProcess codingProcess) noexcept
{
    return codingProcess == JpegCodingProcess::ProgressiveDctHuffman ||
           codingProcess == JpegCodingProcess::ProgressiveDctArithmetic;
}

[[nodiscard]] ImageProcessingResult<JpegFrameHeader> frameHeaderError(const ImageProcessingErrorCode errorCode) noexcept
{
    return ImageProcessingResult<JpegFrameHeader>::failure(ImageProcessingError{
        errorCode,
        ImageProcessingStage::JpegStructureValidation,
    });
}

[[nodiscard]] ImageProcessingResult<JpegFrameHeader> frameHeaderPixelLimitError(
    const std::uint64_t observedPixelCount, const std::uint64_t maximumPixelCount) noexcept
{
    return ImageProcessingResult<JpegFrameHeader>::failure(ImageProcessingError{
        ImageProcessingErrorCode::PixelCountLimitExceeded,
        ImageProcessingStage::JpegStructureValidation,
        {},
        JpegResourceLimitViolation{
            JpegResourceLimit::PixelCount,
            observedPixelCount,
            maximumPixelCount,
        },
    });
}

[[nodiscard]] ImageProcessingResult<JpegFrameHeader> parseFrameHeader(const std::span<const std::byte> encodedJpeg,
                                                                      const JpegMarkerReference &marker,
                                                                      const JpegCodingProcess codingProcess,
                                                                      const std::uint64_t maximumPixelCount)
{
    constexpr std::uint64_t fixedPayloadLengthBytes = 6;
    constexpr std::uint64_t componentPayloadLengthBytes = 3;
    if (marker.payloadRange.lengthBytes < fixedPayloadLengthBytes)
    {
        return frameHeaderError(ImageProcessingErrorCode::MalformedJpegStructure);
    }

    const auto payloadOffset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
    const auto componentCount = byteValue(encodedJpeg[payloadOffset + 5]);
    if (componentCount == 0 ||
        marker.payloadRange.lengthBytes != fixedPayloadLengthBytes + (componentPayloadLengthBytes * componentCount))
    {
        return frameHeaderError(ImageProcessingErrorCode::MalformedJpegStructure);
    }

    const auto heightPixels = tryReadBigEndianUnsigned16(encodedJpeg, payloadOffset + 1);
    const auto widthPixels = tryReadBigEndianUnsigned16(encodedJpeg, payloadOffset + 3);
    if (!heightPixels.has_value() || !widthPixels.has_value() || *widthPixels == 0)
    {
        return frameHeaderError(ImageProcessingErrorCode::MalformedJpegStructure);
    }
    if (*heightPixels == 0)
    {
        // T.81 permits height to be deferred to DNL, but libjpeg-turbo 3.2
        // explicitly does not support DNL. Do not invent a header-rewriting
        // compatibility path or misreport this valid feature as corruption.
        return frameHeaderError(ImageProcessingErrorCode::UnsupportedJpegDeferredHeight);
    }

    const auto samplePrecisionBits = byteValue(encodedJpeg[payloadOffset]);
    const auto isSupportedPrecision = codingProcess == JpegCodingProcess::BaselineDctHuffman
                                          ? samplePrecisionBits == 8
                                          : samplePrecisionBits == 8 || samplePrecisionBits == 12;
    if (!isSupportedPrecision)
    {
        return frameHeaderError(ImageProcessingErrorCode::UnsupportedJpegSamplePrecision);
    }

    if (componentCount != 1 && componentCount != 3 && componentCount != 4)
    {
        return frameHeaderError(ImageProcessingErrorCode::UnsupportedJpegComponentOrganization);
    }

    const auto pixelCount = static_cast<std::uint64_t>(*widthPixels) * *heightPixels;
    if (pixelCount > maximumPixelCount)
    {
        return frameHeaderPixelLimitError(pixelCount, maximumPixelCount);
    }

    std::vector<JpegComponentDescription> components;
    components.reserve(componentCount);
    std::array<bool, 256> observedComponentIdentifiers{};
    for (std::uint16_t componentIndex = 0; componentIndex < componentCount; ++componentIndex)
    {
        const auto componentOffset = payloadOffset + 6 + (componentIndex * 3);
        const auto componentIdentifier = byteValue(encodedJpeg[componentOffset]);
        const auto packedSamplingFactors = byteValue(encodedJpeg[componentOffset + 1]);
        const auto horizontalSamplingFactor = static_cast<std::uint8_t>(packedSamplingFactors >> 4);
        const auto verticalSamplingFactor = static_cast<std::uint8_t>(packedSamplingFactors & 0x0F);
        const auto quantizationTableSelector = byteValue(encodedJpeg[componentOffset + 2]);
        // T.81 Table B.2 bounds each sampling factor independently to 1..4
        // and DCT quantization-table destinations to 0..3. The codec owns
        // table contents and entropy validity, not this outer-header bound.
        if (observedComponentIdentifiers[componentIdentifier] || horizontalSamplingFactor == 0 ||
            horizontalSamplingFactor > 4 || verticalSamplingFactor == 0 || verticalSamplingFactor > 4 ||
            quantizationTableSelector > 3)
        {
            return frameHeaderError(ImageProcessingErrorCode::MalformedJpegStructure);
        }

        observedComponentIdentifiers[componentIdentifier] = true;
        components.push_back({
            componentIdentifier,
            horizontalSamplingFactor,
            verticalSamplingFactor,
            quantizationTableSelector,
        });
    }

    return ImageProcessingResult<JpegFrameHeader>::success(JpegFrameHeader{
        codingProcess,
        samplePrecisionBits,
        ImageDimensions{PixelWidth{*widthPixels}, PixelHeight{*heightPixels}},
        std::move(components),
        marker.encodedRange,
    });
}

[[nodiscard]] bool isValidScanHeader(const std::span<const std::byte> encodedJpeg,
                                     const JpegMarkerReference &marker) noexcept
{
    constexpr std::uint64_t fixedPayloadLengthBytes = 4;
    constexpr std::uint64_t componentPayloadLengthBytes = 2;
    if (marker.payloadRange.lengthBytes < fixedPayloadLengthBytes)
    {
        return false;
    }

    const auto payloadOffset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
    const auto componentCount = byteValue(encodedJpeg[payloadOffset]);
    return componentCount != 0 &&
           marker.payloadRange.lengthBytes == fixedPayloadLengthBytes + (componentPayloadLengthBytes * componentCount);
}
} // namespace

ImageProcessingResult<JpegMarkerInventory> JpegSegmentScanner::scan(
    const std::span<const std::byte> encodedJpeg, const jpg_spinner::domain::JpegResourceLimits &resourceLimits)
{
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    const auto encodedSourceLengthBytes = static_cast<std::uint64_t>(encodedJpeg.size());
    if (encodedSourceLengthBytes > resourceLimits.maximumEncodedFileLengthBytes)
    {
        return encodedFileTooLarge(encodedSourceLengthBytes, resourceLimits.maximumEncodedFileLengthBytes);
    }

    if (encodedJpeg.size() < 2 || byteValue(encodedJpeg[0]) != markerPrefix ||
        byteValue(encodedJpeg[1]) != startOfImageMarkerCode)
    {
        return malformedJpegStructure();
    }

    std::vector<JpegMarkerReference> markers;
    markers.push_back({
        startOfImageMarkerCode,
        EncodedByteRange{0, 2},
        EncodedByteRange{2, 0},
    });
    std::vector<EncodedByteRange> entropyCodedDataRanges;
    JpegMetadataSegmentInventory metadataSegments;
    std::optional<JpegFrameHeader> frameHeader;
    std::uint32_t scanCount = 0;
    std::uint64_t metadataEncodedLengthBytes = 0;
    std::size_t cursor = 2;
    bool isReadingEntropyCodedData = false;
    std::size_t entropyCodedDataOffset = 0;

    while (cursor < encodedJpeg.size())
    {
        if (isReadingEntropyCodedData)
        {
            if (byteValue(encodedJpeg[cursor]) != markerPrefix)
            {
                ++cursor;
                continue;
            }

            const auto markerFillOffset = cursor;
            auto markerCodeOffset = cursor + 1;
            while (markerCodeOffset < encodedJpeg.size() && byteValue(encodedJpeg[markerCodeOffset]) == markerPrefix)
            {
                ++markerCodeOffset;
            }
            if (markerCodeOffset >= encodedJpeg.size())
            {
                return malformedJpegStructure();
            }

            const auto markerCode = byteValue(encodedJpeg[markerCodeOffset]);
            if (markerCode == stuffedEntropyByteCode)
            {
                cursor = markerCodeOffset + 1;
                continue;
            }

            const auto markerOffset = markerCodeOffset - 1;
            if (isRestartMarker(markerCode))
            {
                markers.push_back({
                    markerCode,
                    EncodedByteRange{static_cast<std::uint64_t>(markerOffset), 2},
                    EncodedByteRange{static_cast<std::uint64_t>(markerCodeOffset + 1), 0},
                });
                cursor = markerCodeOffset + 1;
                continue;
            }

            // Fill bytes precede the next marker and are not part of the
            // entropy-coded range. Subtraction is safe because both offsets
            // were produced by monotonic traversal of the same span.
            entropyCodedDataRanges.push_back({
                static_cast<std::uint64_t>(entropyCodedDataOffset),
                static_cast<std::uint64_t>(markerFillOffset - entropyCodedDataOffset),
            });
            isReadingEntropyCodedData = false;
            cursor = markerOffset;
            continue;
        }

        if (byteValue(encodedJpeg[cursor]) != markerPrefix)
        {
            return malformedJpegStructure();
        }

        auto markerCodeOffset = cursor + 1;
        while (markerCodeOffset < encodedJpeg.size() && byteValue(encodedJpeg[markerCodeOffset]) == markerPrefix)
        {
            ++markerCodeOffset;
        }
        if (markerCodeOffset >= encodedJpeg.size())
        {
            return malformedJpegStructure();
        }

        const auto markerCode = byteValue(encodedJpeg[markerCodeOffset]);
        if (markerCode == stuffedEntropyByteCode)
        {
            return malformedJpegStructure();
        }
        const auto markerOffset = markerCodeOffset - 1;
        cursor = markerCodeOffset + 1;

        if (markerCode == endOfImageMarkerCode)
        {
            markers.push_back({
                markerCode,
                EncodedByteRange{static_cast<std::uint64_t>(markerOffset), 2},
                EncodedByteRange{static_cast<std::uint64_t>(cursor), 0},
            });
            if (!frameHeader.has_value() || scanCount == 0)
            {
                return malformedJpegStructure();
            }

            if (!validateAndOrderIccProfileChunks(metadataSegments.iccProfileChunks))
            {
                return scannerError(ImageProcessingErrorCode::MalformedImageMetadata);
            }
            if (!validateAndOrderExtendedXmpChunks(metadataSegments.extendedXmpChunks))
            {
                return scannerError(ImageProcessingErrorCode::MalformedImageMetadata);
            }

            switch (classifyJumbfEnvelope(encodedJpeg, markers))
            {
            case JumbfEnvelopeClassification::Malformed:
                return scannerError(ImageProcessingErrorCode::MalformedImageMetadata);
            case JumbfEnvelopeClassification::C2paManifestStore:
                // A coefficient transform changes bytes covered by the C2PA
                // asset binding. This application has no signing identity and
                // must not silently strip or misrepresent those credentials.
                return scannerError(ImageProcessingErrorCode::ContentCredentialsWouldBeInvalidated);
            case JumbfEnvelopeClassification::UnsupportedJumbf:
                return scannerError(ImageProcessingErrorCode::UnsupportedJumbfMetadata);
            case JumbfEnvelopeClassification::Absent:
                break;
            }

            return ImageProcessingResult<JpegMarkerInventory>::success(JpegMarkerInventory{
                encodedSourceLengthBytes,
                std::move(*frameHeader),
                scanCount,
                metadataEncodedLengthBytes,
                static_cast<std::uint64_t>(markerOffset),
                std::move(markers),
                std::move(entropyCodedDataRanges),
                std::move(metadataSegments),
            });
        }

        if (markerCode == startOfImageMarkerCode || isRestartMarker(markerCode))
        {
            // SOI is unique and RSTn is legal only inside entropy-coded data.
            return malformedJpegStructure();
        }

        if (markerCode == temporaryMarkerCode)
        {
            markers.push_back({
                markerCode,
                EncodedByteRange{static_cast<std::uint64_t>(markerOffset), 2},
                EncodedByteRange{static_cast<std::uint64_t>(cursor), 0},
            });
            continue;
        }

        const auto declaredLength = tryReadBigEndianUnsigned16(encodedJpeg, cursor);
        if (!declaredLength.has_value() || *declaredLength < 2 || *declaredLength > encodedJpeg.size() - cursor)
        {
            return malformedJpegStructure();
        }

        const auto payloadOffset = cursor + 2;
        const auto payloadLength = static_cast<std::size_t>(*declaredLength - 2);
        const auto markerEndOffset = cursor + *declaredLength;
        const JpegMarkerReference marker{
            markerCode,
            EncodedByteRange{
                static_cast<std::uint64_t>(markerOffset),
                static_cast<std::uint64_t>(markerEndOffset - markerOffset),
            },
            EncodedByteRange{
                static_cast<std::uint64_t>(payloadOffset),
                static_cast<std::uint64_t>(payloadLength),
            },
        };

        if (isMetadataMarker(markerCode))
        {
            const auto maximumMetadataLengthBytes = resourceLimits.maximumMetadataLengthBytes;
            if (metadataEncodedLengthBytes > maximumMetadataLengthBytes ||
                marker.encodedRange.lengthBytes > maximumMetadataLengthBytes - metadataEncodedLengthBytes)
            {
                // The marker is already proven contained in the source, so
                // this sum cannot overflow uint64_t after a successful
                // encoded-length conversion.
                return scannerResourceLimitError(
                    ImageProcessingErrorCode::MetadataLengthLimitExceeded, JpegResourceLimit::MetadataLengthBytes,
                    metadataEncodedLengthBytes + marker.encodedRange.lengthBytes, maximumMetadataLengthBytes);
            }
            metadataEncodedLengthBytes += marker.encodedRange.lengthBytes;
        }
        if (markerPayloadStartsWith(encodedJpeg, marker, applicationSegment1MarkerCode, exifIdentifier))
        {
            metadataSegments.exifTiffDataRanges.push_back(EncodedByteRange{
                marker.payloadRange.offsetBytes + exifIdentifier.size(),
                marker.payloadRange.lengthBytes - exifIdentifier.size(),
            });
        }
        if (markerPayloadStartsWith(encodedJpeg, marker, applicationSegment1MarkerCode, standardXmpIdentifier))
        {
            metadataSegments.standardXmpPacketRanges.push_back(EncodedByteRange{
                marker.payloadRange.offsetBytes + standardXmpIdentifier.size(),
                marker.payloadRange.lengthBytes - standardXmpIdentifier.size(),
            });
        }
        if (markerPayloadStartsWith(encodedJpeg, marker, applicationSegment1MarkerCode, extendedXmpIdentifier))
        {
            if (marker.payloadRange.lengthBytes < extendedXmpChunkHeaderLengthBytes)
            {
                return scannerError(ImageProcessingErrorCode::MalformedImageMetadata);
            }

            const auto extendedXmpPayloadOffset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
            const auto guidOffset = extendedXmpPayloadOffset + extendedXmpIdentifier.size();
            ExtendedXmpGuid packetGuid{};
            for (std::size_t characterIndex = 0; characterIndex < packetGuid.uppercaseMd5HexadecimalCharacters.size();
                 ++characterIndex)
            {
                packetGuid.uppercaseMd5HexadecimalCharacters[characterIndex] =
                    static_cast<char>(byteValue(encodedJpeg[guidOffset + characterIndex]));
            }

            const auto completePacketLengthBytes = tryReadBigEndianUnsigned32(
                encodedJpeg, guidOffset + packetGuid.uppercaseMd5HexadecimalCharacters.size());
            const auto chunkOffsetBytes = tryReadBigEndianUnsigned32(
                encodedJpeg, guidOffset + packetGuid.uppercaseMd5HexadecimalCharacters.size() + 4);
            if (!completePacketLengthBytes.has_value() || !chunkOffsetBytes.has_value())
            {
                return scannerError(ImageProcessingErrorCode::MalformedImageMetadata);
            }

            metadataSegments.extendedXmpChunks.push_back(ExtendedXmpChunkReference{
                packetGuid,
                *completePacketLengthBytes,
                *chunkOffsetBytes,
                EncodedByteRange{
                    marker.payloadRange.offsetBytes + extendedXmpChunkHeaderLengthBytes,
                    marker.payloadRange.lengthBytes - extendedXmpChunkHeaderLengthBytes,
                },
            });
        }
        if (markerPayloadStartsWith(encodedJpeg, marker, applicationSegment2MarkerCode, mpFormatIdentifier))
        {
            // CIPA DC-007 defines MP offsets and cross-image relationships
            // beyond this first JPEG. A single-image coefficient transform
            // cannot preserve those relationships truthfully.
            return scannerError(ImageProcessingErrorCode::MultiPictureJpegNotSupported);
        }
        if (hasIccProfileIdentifier(encodedJpeg, marker))
        {
            if (marker.payloadRange.lengthBytes < iccProfileChunkHeaderLengthBytes)
            {
                return scannerError(ImageProcessingErrorCode::MalformedImageMetadata);
            }

            const auto chunkHeaderOffset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
            metadataSegments.iccProfileChunks.push_back(IccProfileChunkReference{
                byteValue(encodedJpeg[chunkHeaderOffset + 12]),
                byteValue(encodedJpeg[chunkHeaderOffset + 13]),
                EncodedByteRange{
                    marker.payloadRange.offsetBytes + iccProfileChunkHeaderLengthBytes,
                    marker.payloadRange.lengthBytes - iccProfileChunkHeaderLengthBytes,
                },
            });
        }
        markers.push_back(marker);
        cursor = markerEndOffset;

        const auto supportedCodingProcess = supportedCodingProcessFor(markerCode);
        if (supportedCodingProcess.has_value())
        {
            if (frameHeader.has_value())
            {
                return malformedJpegStructure();
            }
            auto frameHeaderResult =
                parseFrameHeader(encodedJpeg, marker, *supportedCodingProcess, resourceLimits.maximumPixelCount);
            if (const auto *const error = frameHeaderResult.errorIfPresent(); error != nullptr)
            {
                return ImageProcessingResult<JpegMarkerInventory>::failure(*error);
            }
            frameHeader = std::move(*frameHeaderResult.valueIfPresent());
        }
        else if (isUnsupportedCodingProcessMarker(markerCode))
        {
            return scannerError(ImageProcessingErrorCode::UnsupportedJpegCodingProcess);
        }
        else if (markerCode == startOfScanMarkerCode)
        {
            if (!frameHeader.has_value() || !isValidScanHeader(encodedJpeg, marker) ||
                scanCount == std::numeric_limits<std::uint32_t>::max())
            {
                return malformedJpegStructure();
            }

            if (isProgressiveCodingProcess(frameHeader->codingProcess) &&
                scanCount >= resourceLimits.maximumProgressiveScanCount)
            {
                return scannerResourceLimitError(ImageProcessingErrorCode::ProgressiveScanCountLimitExceeded,
                                                 JpegResourceLimit::ProgressiveScanCount,
                                                 static_cast<std::uint64_t>(scanCount) + 1,
                                                 resourceLimits.maximumProgressiveScanCount);
            }

            ++scanCount;
            isReadingEntropyCodedData = true;
            entropyCodedDataOffset = cursor;
        }
    }

    return malformedJpegStructure();
}
} // namespace jpg_spinner::jpeg::internal
