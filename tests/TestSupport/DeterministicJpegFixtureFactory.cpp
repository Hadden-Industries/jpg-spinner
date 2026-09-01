#include "DeterministicJpegFixtureFactory.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <jpeglib.h>

#include <array>
#include <csetjmp>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>

namespace jpg_spinner::test_support
{
    namespace
    {
        constexpr JDIMENSION fixtureWidth = 31;
        constexpr JDIMENSION fixtureHeight = 19;
        constexpr int fixtureQuality = 90;
        constexpr std::size_t rgbChannelCount = 3;

        // jmp_buf carries an implementation-required alignment that can add
        // harmless tail padding to this private interop structure.
#pragma warning(push)
#pragma warning(disable : 4324)
        struct JpegErrorManager final
        {
            jpeg_error_mgr base{};
            std::jmp_buf jumpBuffer{};
            std::array<char, JMSG_LENGTH_MAX> message{};
        };
#pragma warning(pop)

        [[noreturn]] void handleJpegError(j_common_ptr codec)
        {
            auto* const errorManager = reinterpret_cast<JpegErrorManager*>(codec->err);
            (*codec->err->format_message)(codec, errorManager->message.data());
            std::longjmp(errorManager->jumpBuffer, 1);
        }

        void copyFailureMessage(
            std::array<char, JMSG_LENGTH_MAX>& destination,
            const char* source) noexcept
        {
            static_cast<void>(strncpy_s(
                destination.data(),
                destination.size(),
                source,
                _TRUNCATE));
        }

        std::array<unsigned char, 32> createExifOrientationMarker(
            std::uint16_t exifOrientation) noexcept
        {
            // APP1 payload: "Exif\0\0", then a little-endian TIFF header with
            // one IFD0 SHORT entry for Exif.Image.Orientation (tag 0x0112).
            return {
                'E', 'x', 'i', 'f', 0x00, 0x00,
                'I', 'I', 0x2a, 0x00,
                0x08, 0x00, 0x00, 0x00,
                0x01, 0x00,
                0x12, 0x01,
                0x03, 0x00,
                0x01, 0x00, 0x00, 0x00,
                static_cast<unsigned char>(exifOrientation & 0xff),
                static_cast<unsigned char>((exifOrientation >> 8) & 0xff),
                0x00, 0x00,
                0x00, 0x00, 0x00, 0x00,
            };
        }

        void configureSamplingFactors(
            jpeg_compress_struct& encoder,
            JpegChromaSubsampling chromaSubsampling) noexcept
        {
            // jpeg_set_defaults creates three components for JCS_RGB input.
            // Chroma components remain 1 x 1 while the luminance factors state
            // the exact Y:Cb:Cr ratio required by each fixture.
            encoder.comp_info[1].h_samp_factor = 1;
            encoder.comp_info[1].v_samp_factor = 1;
            encoder.comp_info[2].h_samp_factor = 1;
            encoder.comp_info[2].v_samp_factor = 1;

            switch (chromaSubsampling)
            {
            case JpegChromaSubsampling::ycbcr444:
                encoder.comp_info[0].h_samp_factor = 1;
                encoder.comp_info[0].v_samp_factor = 1;
                break;
            case JpegChromaSubsampling::ycbcr422:
                encoder.comp_info[0].h_samp_factor = 2;
                encoder.comp_info[0].v_samp_factor = 1;
                break;
            case JpegChromaSubsampling::ycbcr420:
                encoder.comp_info[0].h_samp_factor = 2;
                encoder.comp_info[0].v_samp_factor = 2;
                break;
            default:
                // The public wrapper validates the closed enum domain before
                // entering this no-throw C codec boundary.
                encoder.comp_info[0].h_samp_factor = 1;
                encoder.comp_info[0].v_samp_factor = 1;
                break;
            }
        }

        bool encodeFixture(
            JpegChromaSubsampling chromaSubsampling,
            std::uint16_t exifOrientation,
            unsigned char*& encodedBytes,
            unsigned long& encodedByteCount,
            std::array<char, JMSG_LENGTH_MAX>& failureMessage)
        {
            jpeg_compress_struct encoder{};
            JpegErrorManager errorManager{};
            unsigned char* scanlineBytes = nullptr;
            bool encoderCreated = false;

            encoder.err = jpeg_std_error(&errorManager.base);
            errorManager.base.error_exit = handleJpegError;
            // MSVC warns about every setjmp in C++ even when, as here, only
            // trivially destructible state exists across the jump boundary.
#pragma warning(suppress : 4611)
            if (setjmp(errorManager.jumpBuffer) != 0)
            {
                if (scanlineBytes != nullptr)
                {
                    std::free(scanlineBytes);
                }
                if (encoderCreated)
                {
                    jpeg_destroy_compress(&encoder);
                }
                if (encodedBytes != nullptr)
                {
                    std::free(encodedBytes);
                    encodedBytes = nullptr;
                }
                failureMessage = errorManager.message;
                return false;
            }

            jpeg_create_compress(&encoder);
            encoderCreated = true;
            jpeg_mem_dest(&encoder, &encodedBytes, &encodedByteCount);

            encoder.image_width = fixtureWidth;
            encoder.image_height = fixtureHeight;
            encoder.input_components = static_cast<int>(rgbChannelCount);
            encoder.in_color_space = JCS_RGB;
            jpeg_set_defaults(&encoder);

            // The pinned libjpeg-turbo version deterministically derives the
            // same standard luminance/chrominance tables from this fixed
            // quality. Dynamic Huffman optimization and progressive scan
            // generation remain disabled so bytes do not depend on image
            // statistics or scan-script heuristics.
            jpeg_set_quality(&encoder, fixtureQuality, TRUE);
            encoder.optimize_coding = FALSE;
            encoder.arith_code = FALSE;
            encoder.dct_method = JDCT_ISLOW;
            encoder.restart_interval = 0;
            configureSamplingFactors(encoder, chromaSubsampling);

            jpeg_start_compress(&encoder, TRUE);
            const auto exifMarker = createExifOrientationMarker(exifOrientation);
            jpeg_write_marker(
                &encoder,
                JPEG_APP0 + 1,
                exifMarker.data(),
                static_cast<unsigned int>(exifMarker.size()));

            constexpr std::size_t scanlineByteCount =
                static_cast<std::size_t>(fixtureWidth) * rgbChannelCount;
            scanlineBytes = static_cast<unsigned char*>(std::malloc(scanlineByteCount));
            if (scanlineBytes == nullptr)
            {
                copyFailureMessage(failureMessage, "Could not allocate the fixture scanline.");
                jpeg_destroy_compress(&encoder);
                encoderCreated = false;
                if (encodedBytes != nullptr)
                {
                    std::free(encodedBytes);
                    encodedBytes = nullptr;
                }
                return false;
            }

            while (encoder.next_scanline < fixtureHeight)
            {
                const auto y = static_cast<unsigned int>(encoder.next_scanline);
                for (unsigned int x = 0; x < fixtureWidth; ++x)
                {
                    const auto pixelOffset = static_cast<std::size_t>(x) * rgbChannelCount;
                    scanlineBytes[pixelOffset] =
                        static_cast<unsigned char>(17 + (5 * x) + (3 * y));
                    scanlineBytes[pixelOffset + 1] =
                        static_cast<unsigned char>(29 + (2 * x) + (7 * y));
                    scanlineBytes[pixelOffset + 2] =
                        static_cast<unsigned char>(43 + (6 * x) + y);
                }

                JSAMPROW scanline = scanlineBytes;
                if (jpeg_write_scanlines(&encoder, &scanline, 1) != 1)
                {
                    copyFailureMessage(
                        failureMessage,
                        "libjpeg suspended while writing an in-memory fixture scanline.");
                    std::free(scanlineBytes);
                    scanlineBytes = nullptr;
                    jpeg_destroy_compress(&encoder);
                    encoderCreated = false;
                    if (encodedBytes != nullptr)
                    {
                        std::free(encodedBytes);
                        encodedBytes = nullptr;
                    }
                    return false;
                }
            }

            jpeg_finish_compress(&encoder);
            jpeg_destroy_compress(&encoder);
            encoderCreated = false;
            std::free(scanlineBytes);
            return true;
        }
    }

    std::vector<std::byte> DeterministicJpegFixtureFactory::createEncodedJpeg(
        JpegChromaSubsampling chromaSubsampling,
        std::uint16_t exifOrientation)
    {
        if (exifOrientation < 1 || exifOrientation > 8)
        {
            throw std::invalid_argument("Exif orientation must be in the inclusive range 1 through 8.");
        }

        // Validate enum input before entering the C-style codec boundary. The
        // raw function contains only trivial state between setjmp and libjpeg
        // calls, so libjpeg's documented longjmp error protocol cannot skip a
        // C++ object that requires destruction.
        switch (chromaSubsampling)
        {
        case JpegChromaSubsampling::ycbcr444:
        case JpegChromaSubsampling::ycbcr422:
        case JpegChromaSubsampling::ycbcr420:
            break;
        default:
            throw std::invalid_argument("Unsupported JPEG chroma subsampling value.");
        }

        unsigned char* encodedBytes = nullptr;
        unsigned long encodedByteCount = 0;
        std::array<char, JMSG_LENGTH_MAX> failureMessage{};
        if (!encodeFixture(
                chromaSubsampling,
                exifOrientation,
                encodedBytes,
                encodedByteCount,
                failureMessage))
        {
            throw std::runtime_error(
                failureMessage[0] == '\0' ?
                    "libjpeg could not create the deterministic fixture." :
                    failureMessage.data());
        }

        const std::unique_ptr<unsigned char, decltype(&std::free)> ownedEncodedBytes(
            encodedBytes,
            &std::free);
        const auto* const firstByte =
            reinterpret_cast<const std::byte*>(ownedEncodedBytes.get());
        return std::vector<std::byte>(firstByte, firstByte + encodedByteCount);
    }
}
