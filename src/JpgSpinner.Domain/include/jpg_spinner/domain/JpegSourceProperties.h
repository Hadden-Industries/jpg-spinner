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
/// These are encoded source facts, not an inference about its color space.
struct JpegComponentDescription final
{
    std::uint8_t componentIdentifier;
    std::uint8_t horizontalSamplingFactor;
    std::uint8_t verticalSamplingFactor;
    std::uint8_t quantizationTableSelector;

    [[nodiscard]] friend constexpr bool operator==(const JpegComponentDescription &,
                                                   const JpegComponentDescription &) noexcept = default;
};

/// Owned source facts suitable for review without exposing parser ranges,
/// native metadata objects or borrowed encoded bytes.
struct JpegSourceProperties final
{
    JpegCodingProcess codingProcess;
    std::uint8_t samplePrecisionBits;
    ImageDimensions dimensions;
    std::vector<JpegComponentDescription> components;
};
} // namespace jpg_spinner::domain
