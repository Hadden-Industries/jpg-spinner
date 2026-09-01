#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace jpg_spinner::test_support
{
    // Names describe the JPEG's Y:Cb:Cr sampling ratio rather than an encoder
    // implementation detail, so fixture call sites state the format invariant
    // they intend to exercise.
    enum class JpegChromaSubsampling
    {
        ycbcr444,
        ycbcr422,
        ycbcr420,
    };

    class DeterministicJpegFixtureFactory final
    {
    public:
        DeterministicJpegFixtureFactory() = delete;

        // Creates a 31 x 19 baseline JPEG through libjpeg's public API. Source
        // pixels are deliberately asymmetric and use these exact expressions:
        //   red   = 17 + (5 * x) + (3 * y)
        //   green = 29 + (2 * x) + (7 * y)
        //   blue  = 43 + (6 * x) + y
        // The dimensions and formula distinguish every Exif orientation and
        // keep all channels within [0, 255]. Encoding uses quality 90, the
        // resulting fixed standard quantization tables, slow integer DCT,
        // default Huffman tables, one sequential scan, and no timestamp.
        [[nodiscard]] static std::vector<std::byte> createEncodedJpeg(
            JpegChromaSubsampling chromaSubsampling,
            std::uint16_t exifOrientation);
    };
}
