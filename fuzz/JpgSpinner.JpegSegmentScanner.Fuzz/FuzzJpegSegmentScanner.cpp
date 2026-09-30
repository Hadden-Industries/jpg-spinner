#include "internal/JpegSegmentScanner.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

#if !defined(__SANITIZE_ADDRESS__)
#error The scanner fuzz target requires AddressSanitizer instrumentation.
#endif

namespace
{
// Unlike assert(), this invariant remains active when NDEBUG is defined.
void requireInvariant(const bool condition) noexcept
{
    if (!condition)
    {
        std::abort();
    }
}
} // namespace

// Only the bounded scanner is exercised. A thrown exception is a reproducible
// fuzz failure, never an exception crossing libFuzzer's C callback boundary.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, const std::size_t size) noexcept
{
    try
    {
        using jpg_spinner::jpeg::internal::JpegSegmentScanner;
        const auto bytes = std::as_bytes(std::span(data, size));
        const auto result = JpegSegmentScanner::scan(bytes);
        if (const auto *inventory = result.valueIfPresent())
        {
            requireInvariant(inventory->encodedSourceLengthBytes() == size);
            requireInvariant(inventory->frameHeader().encodedRange.isContainedWithin(size));
            requireInvariant(inventory->trailingDataRange().isContainedWithin(size));
            for (const auto &marker : inventory->markers())
            {
                requireInvariant(marker.encodedRange.isContainedWithin(size));
                requireInvariant(marker.payloadRange.isContainedWithin(size));
            }
            for (const auto &range : inventory->entropyCodedDataRanges())
                requireInvariant(range.isContainedWithin(size));
            for (const auto &range : inventory->exifTiffDataRanges())
                requireInvariant(range.isContainedWithin(size));
            for (const auto &range : inventory->standardXmpPacketRanges())
                requireInvariant(range.isContainedWithin(size));
            for (const auto &chunk : inventory->iccProfileChunks())
                requireInvariant(chunk.profileDataRange.isContainedWithin(size));
            for (const auto &chunk : inventory->extendedXmpChunks())
                requireInvariant(chunk.packetDataRange.isContainedWithin(size));

            const auto repeatedResult = JpegSegmentScanner::scan(bytes);
            requireInvariant(repeatedResult.valueIfPresent() != nullptr);
            requireInvariant(*inventory == *repeatedResult.valueIfPresent());
        }
    }
    catch (...)
    {
        std::abort();
    }
    return 0;
}
