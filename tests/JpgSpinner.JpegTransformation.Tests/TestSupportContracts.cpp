#define NOMINMAX

#include "CoefficientDigest.h"
#include "DeterministicJpegFixtureFactory.h"
#include "TemporaryDirectory.h"

#include <catch2/catch_test_macros.hpp>
#include <exiv2/exiv2.hpp>

#include <Windows.h>
#include <bcrypt.h>

#include <array>
#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <jpeglib.h>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#pragma comment(lib, "bcrypt.lib")

namespace
{
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

    struct DecodedJpeg final
    {
        std::uint32_t width{};
        std::uint32_t height{};
        std::array<std::array<int, 2>, 3> samplingFactors{};
        std::vector<std::uint8_t> rgbPixels;
    };

    DecodedJpeg decodeJpeg(std::span<const std::byte> encodedJpeg)
    {
        if (encodedJpeg.size() > std::numeric_limits<unsigned long>::max())
        {
            throw std::invalid_argument("The JPEG fixture exceeds libjpeg's in-memory source limit.");
        }

        jpeg_decompress_struct decoder{};
        JpegErrorManager errorManager{};
        decoder.err = jpeg_std_error(&errorManager.base);
        errorManager.base.error_exit = handleJpegError;

        std::uint8_t* decodedPixels = nullptr;
        bool decoderCreated = false;
        // Only trivially destructible state exists across this documented
        // libjpeg jump boundary; suppress MSVC's intentionally broad warning.
#pragma warning(suppress : 4611)
        if (setjmp(errorManager.jumpBuffer) != 0)
        {
            if (decodedPixels != nullptr)
            {
                std::free(decodedPixels);
            }
            if (decoderCreated)
            {
                jpeg_destroy_decompress(&decoder);
            }
            throw std::runtime_error(errorManager.message.data());
        }

        jpeg_create_decompress(&decoder);
        decoderCreated = true;
        jpeg_mem_src(
            &decoder,
            reinterpret_cast<const unsigned char*>(encodedJpeg.data()),
            static_cast<unsigned long>(encodedJpeg.size()));
        if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK)
        {
            jpeg_destroy_decompress(&decoder);
            throw std::runtime_error("libjpeg did not recognize the generated fixture header.");
        }
        if (decoder.num_components != 3)
        {
            jpeg_destroy_decompress(&decoder);
            throw std::runtime_error("The generated fixture is not a three-component JPEG.");
        }

        const auto decodedWidth = static_cast<std::uint32_t>(decoder.image_width);
        const auto decodedHeight = static_cast<std::uint32_t>(decoder.image_height);
        std::array<std::array<int, 2>, 3> samplingFactors{};
        for (int componentIndex = 0; componentIndex < decoder.num_components; ++componentIndex)
        {
            samplingFactors[static_cast<std::size_t>(componentIndex)] = {
                decoder.comp_info[componentIndex].h_samp_factor,
                decoder.comp_info[componentIndex].v_samp_factor,
            };
        }

        decoder.out_color_space = JCS_RGB;
        jpeg_start_decompress(&decoder);
        constexpr std::size_t channelCount = 3;
        const auto rowByteCount = static_cast<std::size_t>(decoder.output_width) * channelCount;
        if (
            decoder.output_height != 0 &&
            rowByteCount > std::numeric_limits<std::size_t>::max() /
                static_cast<std::size_t>(decoder.output_height))
        {
            jpeg_destroy_decompress(&decoder);
            throw std::overflow_error("Decoded JPEG dimensions exceed addressable memory.");
        }
        const auto decodedByteCount =
            rowByteCount * static_cast<std::size_t>(decoder.output_height);
        decodedPixels = static_cast<std::uint8_t*>(std::malloc(decodedByteCount));
        if (decodedPixels == nullptr)
        {
            jpeg_destroy_decompress(&decoder);
            throw std::bad_alloc();
        }

        while (decoder.output_scanline < decoder.output_height)
        {
            JSAMPROW row = decodedPixels +
                (static_cast<std::size_t>(decoder.output_scanline) * rowByteCount);
            if (jpeg_read_scanlines(&decoder, &row, 1) != 1)
            {
                std::free(decodedPixels);
                decodedPixels = nullptr;
                jpeg_destroy_decompress(&decoder);
                throw std::runtime_error("libjpeg stopped before decoding every fixture scanline.");
            }
        }

        jpeg_finish_decompress(&decoder);
        jpeg_destroy_decompress(&decoder);
        decoderCreated = false;

        // Construct objects with non-trivial destructors only after the final
        // libjpeg call; a codec error can no longer longjmp across them.
        const std::unique_ptr<std::uint8_t, decltype(&std::free)> ownedDecodedPixels(
            decodedPixels,
            &std::free);
        DecodedJpeg result{};
        result.width = decodedWidth;
        result.height = decodedHeight;
        result.samplingFactors = samplingFactors;
        result.rgbPixels.assign(
            ownedDecodedPixels.get(),
            ownedDecodedPixels.get() + decodedByteCount);
        return result;
    }

    std::string calculateSha256Hex(std::span<const std::byte> bytes)
    {
        if (bytes.size() > std::numeric_limits<ULONG>::max())
        {
            throw std::invalid_argument("The byte sequence exceeds BCrypt's one-shot input limit.");
        }

        BCRYPT_ALG_HANDLE algorithm = nullptr;
        const NTSTATUS openStatus = BCryptOpenAlgorithmProvider(
            &algorithm,
            BCRYPT_SHA256_ALGORITHM,
            nullptr,
            0);
        if (openStatus < 0)
        {
            throw std::runtime_error("BCryptOpenAlgorithmProvider failed for SHA-256.");
        }

        std::array<std::uint8_t, 32> digest{};
        const NTSTATUS hashStatus = BCryptHash(
            algorithm,
            nullptr,
            0,
            reinterpret_cast<PUCHAR>(const_cast<std::byte*>(bytes.data())),
            static_cast<ULONG>(bytes.size()),
            digest.data(),
            static_cast<ULONG>(digest.size()));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (hashStatus < 0)
        {
            throw std::runtime_error("BCryptHash failed for SHA-256.");
        }

        constexpr std::string_view hexadecimalDigits = "0123456789abcdef";
        std::string hexadecimalDigest(digest.size() * 2, '0');
        for (std::size_t index = 0; index < digest.size(); ++index)
        {
            hexadecimalDigest[index * 2] = hexadecimalDigits[digest[index] >> 4];
            hexadecimalDigest[(index * 2) + 1] =
                hexadecimalDigits[digest[index] & 0x0f];
        }
        return hexadecimalDigest;
    }

    std::array<std::uint8_t, 3> decodedPixelAt(
        const DecodedJpeg& decodedJpeg,
        std::uint32_t x,
        std::uint32_t y)
    {
        const auto pixelOffset =
            ((static_cast<std::size_t>(y) * decodedJpeg.width) + x) * 3;
        return {
            decodedJpeg.rgbPixels[pixelOffset],
            decodedJpeg.rgbPixels[pixelOffset + 1],
            decodedJpeg.rgbPixels[pixelOffset + 2],
        };
    }

    void requirePixelNear(
        const DecodedJpeg& decodedJpeg,
        std::uint32_t x,
        std::uint32_t y,
        std::array<std::uint8_t, 3> expectedPixel)
    {
        constexpr int maximumChannelError = 24;
        const auto actualPixel = decodedPixelAt(decodedJpeg, x, y);
        for (std::size_t channelIndex = 0; channelIndex < actualPixel.size(); ++channelIndex)
        {
            INFO("pixel=(" << x << ',' << y << "), channel=" << channelIndex);
            REQUIRE(
                std::abs(
                    static_cast<int>(actualPixel[channelIndex]) -
                    static_cast<int>(expectedPixel[channelIndex])) <=
                maximumChannelError);
        }
    }
}

TEST_CASE(
    "TemporaryDirectory owns exactly one unique child of the test output root",
    "[test-support][filesystem]")
{
    namespace test_support = jpg_spinner::test_support;

    const auto testOutputRoot =
        std::filesystem::current_path() / "test-output-root";
    std::filesystem::create_directories(testOutputRoot);
    const auto sentinelPath = testOutputRoot / "must-remain.txt";
    {
        std::ofstream sentinel(sentinelPath);
        sentinel << "owned by the test output root";
    }

    std::filesystem::path secondOwnedPath;
    {
        test_support::TemporaryDirectory firstDirectory(testOutputRoot);
        test_support::TemporaryDirectory secondDirectory(testOutputRoot);
        secondOwnedPath = secondDirectory.directoryPath();

        REQUIRE(firstDirectory.directoryPath() != secondDirectory.directoryPath());
        REQUIRE(
            std::filesystem::weakly_canonical(firstDirectory.directoryPath()).parent_path() ==
            std::filesystem::weakly_canonical(testOutputRoot));
        REQUIRE(
            std::filesystem::weakly_canonical(secondDirectory.directoryPath()).parent_path() ==
            std::filesystem::weakly_canonical(testOutputRoot));

        std::ofstream(firstDirectory.directoryPath() / "owned.txt") << "owned";
        std::ofstream(secondDirectory.directoryPath() / "owned.txt") << "owned";

        REQUIRE_FALSE(firstDirectory.removeOwnedDirectory());
        REQUIRE_FALSE(firstDirectory.cleanupError());
        REQUIRE_FALSE(std::filesystem::exists(firstDirectory.directoryPath()));
        REQUIRE(std::filesystem::exists(secondDirectory.directoryPath()));
        REQUIRE(std::filesystem::exists(sentinelPath));
    }

    REQUIRE_FALSE(std::filesystem::exists(secondOwnedPath));
    REQUIRE(std::filesystem::exists(sentinelPath));
    REQUIRE(std::filesystem::remove(sentinelPath));
    REQUIRE(std::filesystem::remove(testOutputRoot));
}

TEST_CASE(
    "Deterministic JPEG fixtures preserve reviewed structure, metadata, pixels, and digests",
    "[test-support][jpeg]")
{
    namespace test_support = jpg_spinner::test_support;

    struct FixtureExpectation final
    {
        test_support::JpegChromaSubsampling chromaSubsampling;
        const char* description;
        std::array<int, 2> luminanceSampling;
        const char* encodedSha256;
        const char* coefficientSha256;
    };

    constexpr std::array expectations{
        FixtureExpectation{
            test_support::JpegChromaSubsampling::ycbcr444,
            "4:4:4",
            {1, 1},
            "d588b223e94ccbe8adb4b5beaba86c15768526d39ea4048cccd9c23bf536ba12",
            "86a0d26ae7c72dae9536518d2a4fa04857567cb7f5924318c542beaebd01e0e4",
        },
        FixtureExpectation{
            test_support::JpegChromaSubsampling::ycbcr422,
            "4:2:2",
            {2, 1},
            "f3367f65764acb6769ab855f6b4220721c6eb0010329710c3baa7fae692d751d",
            "cf0bca55b1fe6867cb4d89f3c6619f436e475fec601ec0d5caad6dce37679364",
        },
        FixtureExpectation{
            test_support::JpegChromaSubsampling::ycbcr420,
            "4:2:0",
            {2, 2},
            "32ee31f182b2c4104425f023abfc65ab5d41869989363cfc77139310f9a7e5ec",
            "c8eee24bf302621d11d492e41aaf7527787113726b3b832197f41b4b3a888076",
        },
    };

    for (const auto& expectation : expectations)
    {
        DYNAMIC_SECTION(expectation.description)
        {
            constexpr std::uint16_t requestedExifOrientation = 6;
            const auto encodedJpeg =
                test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
                    expectation.chromaSubsampling,
                    requestedExifOrientation);

            const auto decodedJpeg = decodeJpeg(encodedJpeg);
            REQUIRE(decodedJpeg.width == 31);
            REQUIRE(decodedJpeg.height == 19);
            REQUIRE(decodedJpeg.samplingFactors[0] == expectation.luminanceSampling);
            REQUIRE(decodedJpeg.samplingFactors[1] == std::array{1, 1});
            REQUIRE(decodedJpeg.samplingFactors[2] == std::array{1, 1});

            // These explicit samples independently assert the documented
            // asymmetric source formula after a separate public-API decode.
            requirePixelNear(decodedJpeg, 0, 0, {17, 29, 43});
            requirePixelNear(decodedJpeg, 30, 0, {167, 89, 223});
            requirePixelNear(decodedJpeg, 0, 18, {71, 155, 61});
            requirePixelNear(decodedJpeg, 30, 18, {221, 215, 241});
            requirePixelNear(decodedJpeg, 15, 9, {119, 122, 142});

            auto metadataImage = Exiv2::ImageFactory::open(
                reinterpret_cast<const Exiv2::byte*>(encodedJpeg.data()),
                encodedJpeg.size());
            REQUIRE(metadataImage != nullptr);
            metadataImage->readMetadata();
            const auto orientationPosition = metadataImage->exifData().findKey(
                Exiv2::ExifKey("Exif.Image.Orientation"));
            REQUIRE(orientationPosition != metadataImage->exifData().end());
            REQUIRE(orientationPosition->count() == 1);
            REQUIRE(orientationPosition->toUint32() == requestedExifOrientation);

            const auto encodedSha256 = calculateSha256Hex(encodedJpeg);
            const auto coefficientSha256 =
                test_support::CoefficientDigest::computeSha256Hex(encodedJpeg);
            INFO("encoded SHA-256: " << encodedSha256);
            INFO("coefficient SHA-256: " << coefficientSha256);
            REQUIRE(encodedSha256 == expectation.encodedSha256);
            REQUIRE(coefficientSha256 == expectation.coefficientSha256);
        }
    }
}

TEST_CASE(
    "Deterministic JPEG fixture factory accepts exactly the Exif orientation domain",
    "[test-support][jpeg]")
{
    namespace test_support = jpg_spinner::test_support;

    REQUIRE_THROWS_AS(
        test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444,
            0),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
            test_support::JpegChromaSubsampling::ycbcr444,
            9),
        std::invalid_argument);

    for (const std::uint16_t orientation : {std::uint16_t{1}, std::uint16_t{8}})
    {
        const auto encodedJpeg =
            test_support::DeterministicJpegFixtureFactory::createEncodedJpeg(
                test_support::JpegChromaSubsampling::ycbcr444,
                orientation);
        auto metadataImage = Exiv2::ImageFactory::open(
            reinterpret_cast<const Exiv2::byte*>(encodedJpeg.data()),
            encodedJpeg.size());
        REQUIRE(metadataImage != nullptr);
        metadataImage->readMetadata();
        const auto orientationPosition = metadataImage->exifData().findKey(
            Exiv2::ExifKey("Exif.Image.Orientation"));
        REQUIRE(orientationPosition != metadataImage->exifData().end());
        REQUIRE(orientationPosition->toUint32() == orientation);
    }
}
