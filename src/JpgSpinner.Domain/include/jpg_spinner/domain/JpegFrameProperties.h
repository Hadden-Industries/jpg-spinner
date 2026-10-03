#pragma once

#include "ImageDimensions.h"
#include <cstdint>
#include <vector>

namespace jpg_spinner::domain
{
/// Supported DCT coding processes; independent of JPEG marker byte values.
enum class JpegCodingProcess
{
    BaselineDctHuffman,
    ExtendedSequentialDctHuffman,
    ProgressiveDctHuffman,
    ExtendedSequentialDctArithmetic,
    ProgressiveDctArithmetic,
};

/// One frame component's identity, sampling and quantization-table selection.
/// These are encoded frame facts, not an inference about its color space.
struct JpegComponentDescription final
{
    std::uint8_t componentIdentifier;
    std::uint8_t horizontalSamplingFactor;
    std::uint8_t verticalSamplingFactor;
    std::uint8_t quantizationTableSelector;

    [[nodiscard]] friend constexpr bool operator==(const JpegComponentDescription &,
                                                   const JpegComponentDescription &) noexcept = default;
};

/// Owned JPEG frame-header facts for either source or output, without parser
/// ranges, native metadata objects or borrowed encoded bytes. These are the
/// coding process, precision, dimensions and component parameters, not metadata.
struct JpegFrameProperties final
{
    JpegCodingProcess codingProcess;
    std::uint8_t samplePrecisionBits;
    ImageDimensions dimensions;
    std::vector<JpegComponentDescription> components;

    [[nodiscard]] friend bool operator==(const JpegFrameProperties &, const JpegFrameProperties &) = default;
};
} // namespace jpg_spinner::domain
