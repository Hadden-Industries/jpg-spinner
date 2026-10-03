#include "CoefficientTransformFixture.h"
#include "internal/JpegSegmentScanner.h"
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <catch2/catch_test_macros.hpp>
#include <array>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg;
using namespace jpg_spinner::test_support;

TEST_CASE("One analyzer call returns owned source facts and the exact orientation plan", "[jpeg][metadata][analyzer]")
{
    auto jpeg = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    constexpr std::array<unsigned char, 36> exif{0xff, 0xe1, 0, 34, 'E', 'x', 'i', 'f', 0,    0, 'I', 'I',
                                                 42,   0,    8, 0,  0,   0,   1,   0,   0x12, 1, 3,   0,
                                                 1,    0,    0, 0,  6,   0,   0,   0,   0,    0, 0,   0};
    std::vector<std::byte> segment;
    for (auto byte : exif)
        segment.push_back(static_cast<std::byte>(byte));
    jpeg.insert(jpeg.begin() + 2, segment.begin(), segment.end());
    bool hasThumbnail = false;
    SECTION("No preview requires no thumbnail-removal finding")
    {
    }
    SECTION("A JFXX preview requires explicit review before removal")
    {
        // JFXX RGB extension: one 1x1 pixel, independently framed.
        constexpr std::array<unsigned char, 15> thumbnail{0xff, 0xe0, 0, 13, 'J', 'F', 'X', 'X',
                                                          0,    0x13, 1, 1,  10,  20,  30};
        segment.clear();
        for (auto byte : thumbnail)
            segment.push_back(static_cast<std::byte>(byte));
        jpeg.insert(jpeg.begin() + 2, segment.begin(), segment.end());
        hasThumbnail = true;
    }
    const auto original = jpeg;
    const auto result = JpegImageAnalyzer::analyze(jpeg);
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto &analysis = *result.valueIfPresent();
    CHECK(analysis.authoritativeOrientation == ExifOrientation::RightTop);
    CHECK(analysis.sourceProperties.codingProcess == JpegCodingProcess::BaselineDctHuffman);
    CHECK(analysis.sourceProperties.samplePrecisionBits == 8);
    CHECK(analysis.sourceProperties.dimensions == ImageDimensions{{48}, {32}});
    REQUIRE(analysis.sourceProperties.components.size() == 3);
    CHECK(analysis.sourceProperties.components[0].horizontalSamplingFactor == 2);
    CHECK(analysis.sourceProperties.components[0].verticalSamplingFactor == 2);
    CHECK(analysis.transformPlan.transform == LosslessTransform::Rotate90Clockwise);
    CHECK(analysis.transformPlan.outputDimensions == ImageDimensions{{32}, {48}});
    if (hasThumbnail)
    {
        REQUIRE(analysis.findings.size() == 1);
        CHECK(jpegAnalysisFindingCode(analysis.findings.front()) ==
              JpegAnalysisFindingCode::EmbeddedThumbnailRemovalRequired);
    }
    else
        CHECK(analysis.findings.empty());
    CHECK(jpeg == original);
}

TEST_CASE("Single-component planning uses the codec's eight-pixel transform unit", "[jpeg][metadata][analyzer]")
{
    auto jpeg = createTransformFixture(8, TJSAMP_GRAY, TJCS_GRAY, false, false, 48, 24);
    const auto scan = internal::JpegSegmentScanner::scan(jpeg);
    REQUIRE(scan.valueIfPresent() != nullptr);
    // A one-component scan is non-interleaved; changing its relative sampling
    // factors does not change the encoded sequence of 8x8 coefficient blocks.
    const auto frameOffset = scan.valueIfPresent()->frameHeader().encodedRange.offsetBytes;
    jpeg[static_cast<std::size_t>(frameOffset + 11)] = std::byte{0x22};
    const auto coefficients = readCoefficients(jpeg);
    REQUIRE(coefficients.components.size() == 1);
    REQUIRE(coefficients.components.front().blockColumns == 6);
    REQUIRE(coefficients.components.front().blockRows == 3);
    const auto result = JpegImageAnalyzer::analyze(jpeg);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(result.valueIfPresent()->sourceProperties.components.front().horizontalSamplingFactor == 2);
    CHECK(result.valueIfPresent()->transformPlan.sourceInterleavedMinimumCodedUnitDimensions ==
          InterleavedMinimumCodedUnitDimensions{{8}, {8}});
}
