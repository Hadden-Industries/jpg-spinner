#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace jpg_spinner::test_support
{
    class CoefficientDigest final
    {
    public:
        CoefficientDigest() = delete;

        // Reads libjpeg public virtual coefficient arrays and hashes one
        // canonical, architecture-independent byte stream. Version 1 is:
        //   ASCII "jpg-spinner-coefficients-v1\0";
        //   component count as unsigned 32-bit big-endian;
        //   for each component in JPEG order: its zero-based index, width in
        //   blocks, and height in blocks as unsigned 32-bit big-endian;
        //   every block in row-major order and every natural-order JCOEF as a
        //   signed 16-bit two's-complement big-endian value.
        // Quantization tables and marker bytes are intentionally outside this
        // coefficient-only identity and are asserted separately by callers.
        [[nodiscard]] static std::string computeSha256Hex(
            std::span<const std::byte> encodedJpeg);
    };
}
