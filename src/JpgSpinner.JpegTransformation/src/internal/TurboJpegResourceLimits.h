#pragma once

#include <cstdint>

namespace jpg_spinner::jpeg::internal
{
/// Codec working-memory policy, independent of encoded-file and output bounds.
/// TurboJPEG 3.2.0 multiplies MAXMEMORY by 1,048,576 (turbojpeg.c); this
/// constrains intermediate buffers, not total process memory or output storage.
struct TurboJpegResourceLimits final
{
    const std::int32_t maximumIntermediateBufferMemoryMebibytes;

    [[nodiscard]] static consteval TurboJpegResourceLimits production() noexcept
    {
        return {512};
    }
};
} // namespace jpg_spinner::jpeg::internal
