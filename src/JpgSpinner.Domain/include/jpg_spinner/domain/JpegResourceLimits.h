#pragma once

#include <cstdint>
#include <optional>

namespace jpg_spinner::domain
{
/// Symbolic identity of each independently enforced JPEG resource bound.
/// The unit-bearing enumerators keep diagnostics unambiguous.
enum class JpegResourceLimit
{
    EncodedFileLengthBytes,
    PixelCount,
    MetadataLengthBytes,
    ProgressiveScanCount,
};

struct JpegResourceLimitViolation final
{
    const JpegResourceLimit resourceLimit;

    // Overflow is represented by absence because fabricating a saturated
    // observed value would make the diagnostic numerically false.
    const std::optional<std::uint64_t> observedValue;
    const std::uint64_t maximumValue;
};

/// Application-level limits for untrusted encoded JPEG input. These are
/// deliberately separate from TurboJPEG's mebibyte working-memory
/// parameter, which is an adapter concern rather than an input-size limit.
struct JpegResourceLimits final
{
    const std::uint64_t maximumEncodedFileLengthBytes;
    const std::uint64_t maximumPixelCount;
    const std::uint64_t maximumMetadataLengthBytes;
    const std::uint32_t maximumProgressiveScanCount;

    [[nodiscard]] static consteval JpegResourceLimits production() noexcept
    {
        return {
            512ULL * 1024ULL * 1024ULL,
            268'435'456ULL,
            32ULL * 1024ULL * 1024ULL,
            100U,
        };
    }
};
} // namespace jpg_spinner::domain
