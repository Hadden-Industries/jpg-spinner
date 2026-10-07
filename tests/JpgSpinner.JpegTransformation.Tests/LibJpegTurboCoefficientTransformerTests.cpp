#include "internal/LibJpegTurboCoefficientTransformer.h"
#include "internal/JpegSegmentScanner.h"
#include "CoefficientDigest.h"
#include "DeterministicJpegFixtureFactory.h"
#include "CoefficientTransformFixture.h"

#include <catch2/catch_test_macros.hpp>
#include <jpg_spinner/domain/JpegTransformPlanner.h>
#include <algorithm>
#include <array>
#include <stop_token>

using namespace jpg_spinner::domain;
using namespace jpg_spinner::jpeg::internal;
using namespace jpg_spinner::test_support;

TEST_CASE("An inconsistent planned output cannot publish transformed bytes", "[jpeg][transform][failures]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const JpegTransformPlan plan{LosslessTransform::None,
                                 {{48}, {32}},
                                 {{8}, {8}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    std::array<std::byte, 65536> output{};
    output.fill(std::byte{0x5a});
    const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::OutputValidationFailed);
    CHECK(std::ranges::all_of(output, [](auto value) { return value == std::byte{0x5a}; }));
}

TEST_CASE("Coefficient transforms preserve the MCU restart interval", "[jpeg][transform][restart]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, false, false, 48, 32, 2);
    const JpegTransformPlan plan{LosslessTransform::Rotate90Clockwise,
                                 {{48}, {32}},
                                 {{32}, {48}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    const auto interval = [](std::span<const std::byte> encoded) {
        const auto scan = JpegSegmentScanner::scan(encoded);
        REQUIRE(scan.valueIfPresent() != nullptr);
        for (const auto &marker : scan.valueIfPresent()->markers())
            if (marker.markerCode == 0xdd)
            {
                REQUIRE(marker.payloadRange.lengthBytes == 2);
                return (std::to_integer<unsigned int>(
                            encoded[static_cast<std::size_t>(marker.payloadRange.offsetBytes)])
                        << 8) |
                       std::to_integer<unsigned int>(
                           encoded[static_cast<std::size_t>(marker.payloadRange.offsetBytes + 1)]);
            }
        return 0U;
    };
    REQUIRE(interval(source) == 2);
    std::array<std::byte, 65536> output{};
    const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
    REQUIRE(result.valueIfPresent() != nullptr);
    CHECK(interval(std::span(output).first(*result.valueIfPresent())) == 2);
}

TEST_CASE("Codec failures never publish a partial destination", "[jpeg][transform][failures]")
{
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const JpegTransformPlan plan{LosslessTransform::None,
                                 {{48}, {32}},
                                 {{48}, {32}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    SECTION("undersized nonempty destination")
    {
        std::array<std::byte, 1> output{std::byte{0x5a}};
        const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(output[0] == std::byte{0x5a});
    }
    SECTION("structurally framed but truncated entropy produces a fatal warning")
    {
        const auto scan = JpegSegmentScanner::scan(source);
        REQUIRE(scan.valueIfPresent() != nullptr);
        const auto entropy = scan.valueIfPresent()->entropyCodedDataRanges().front();
        source.erase(source.begin() + static_cast<std::ptrdiff_t>(entropy.offsetBytes),
                     source.begin() + static_cast<std::ptrdiff_t>(entropy.offsetBytes + entropy.lengthBytes));
        REQUIRE(JpegSegmentScanner::scan(source).valueIfPresent() != nullptr);
        std::array<std::byte, 65536> output{};
        output.fill(std::byte{0x5a});
        const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
        REQUIRE(result.errorIfPresent() != nullptr);
        CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::CoefficientTransformationFailed);
        CHECK(std::ranges::all_of(output, [](auto value) { return value == std::byte{0x5a}; }));
    }
}

TEST_CASE("Every spatial operation preserves the independently derived coefficient field",
          "[jpeg][transform][coefficients]")
{
    // DCT cosine parity: reversing x multiplies frequency u by (-1)^u;
    // reversing y multiplies v by (-1)^v. Transposition exchanges u/v,
    // block axes, and quantization-table axes. This is not a codec round trip.
    const std::array operations{LosslessTransform::None,       LosslessTransform::FlipHorizontal,
                                LosslessTransform::Rotate180,  LosslessTransform::FlipVertical,
                                LosslessTransform::Transpose,  LosslessTransform::Rotate90Clockwise,
                                LosslessTransform::Transverse, LosslessTransform::Rotate270Clockwise};
    for (const int precision : {8, 12})
        for (const auto [subsampling, colorSpace] : std::array{
                 std::pair{TJSAMP_444, TJCS_YCbCr}, std::pair{TJSAMP_422, TJCS_YCbCr},
                 std::pair{TJSAMP_420, TJCS_YCbCr}, std::pair{TJSAMP_GRAY, TJCS_GRAY}, std::pair{TJSAMP_444, TJCS_RGB},
                 std::pair{TJSAMP_444, TJCS_CMYK}, std::pair{TJSAMP_420, TJCS_YCCK}})
        {
            const auto source = createTransformFixture(precision, subsampling, colorSpace);
            const auto input = readCoefficients(source);
            REQUIRE(input.precision == precision);
            REQUIRE(input.components.size() ==
                    static_cast<std::size_t>(
                        colorSpace == TJCS_GRAY ? 1 : (colorSpace == TJCS_CMYK || colorSpace == TJCS_YCCK ? 4 : 3)));
            for (std::size_t orientation = 0; orientation < operations.size(); ++orientation)
            {
                CAPTURE(precision, subsampling, colorSpace, orientation);
                const bool swapAxes = orientation >= 4;
                const JpegTransformPlan plan{operations[orientation],
                                             {{48}, {32}},
                                             swapAxes ? ImageDimensions{{32}, {48}} : ImageDimensions{{48}, {32}},
                                             EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                             OutputScanOrganization::PreserveSource,
                                             {{static_cast<std::uint64_t>(tjMCUWidth[subsampling])},
                                              {static_cast<std::uint64_t>(tjMCUHeight[subsampling])}},
                                             {{0}, {0}}};
                std::array<std::byte, 65536> output{};
                const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
                REQUIRE(result.valueIfPresent() != nullptr);
                const auto actual = readCoefficients(std::span(output).first(*result.valueIfPresent()));
                REQUIRE(actual.precision == precision);
                REQUIRE(actual.colorSpace == input.colorSpace);
                REQUIRE(actual.components.size() == input.components.size());
                for (std::size_t componentIndex = 0; componentIndex < input.components.size(); ++componentIndex)
                {
                    const auto &before = input.components[componentIndex];
                    const auto &after = actual.components[componentIndex];
                    REQUIRE(after.blockColumns == (swapAxes ? before.blockRows : before.blockColumns));
                    REQUIRE(after.blockRows == (swapAxes ? before.blockColumns : before.blockRows));
                    for (unsigned int row = 0; row < before.blockRows; ++row)
                        for (unsigned int column = 0; column < before.blockColumns; ++column)
                        {
                            const auto right = before.blockColumns - 1 - column;
                            const auto bottom = before.blockRows - 1 - row;
                            const std::array destinationColumns{column, right, right, column, row, bottom, bottom, row};
                            const std::array destinationRows{row, row, bottom, bottom, column, column, right, right};
                            const auto &sourceBlock = before.blocks[row * before.blockColumns + column];
                            const auto &destinationBlock =
                                after.blocks[destinationRows[orientation] * after.blockColumns +
                                             destinationColumns[orientation]];
                            for (unsigned int verticalFrequency = 0; verticalFrequency < 8; ++verticalFrequency)
                                for (unsigned int horizontalFrequency = 0; horizontalFrequency < 8;
                                     ++horizontalFrequency)
                                {
                                    const auto frequency = verticalFrequency * 8 + horizontalFrequency;
                                    const auto destinationFrequency =
                                        swapAxes ? horizontalFrequency * 8 + verticalFrequency : frequency;
                                    const std::array parity{0U,
                                                            horizontalFrequency,
                                                            horizontalFrequency + verticalFrequency,
                                                            verticalFrequency,
                                                            0U,
                                                            verticalFrequency,
                                                            horizontalFrequency + verticalFrequency,
                                                            horizontalFrequency};
                                    const int sign = parity[orientation] % 2 == 0 ? 1 : -1;
                                    REQUIRE(destinationBlock[destinationFrequency] == sign * sourceBlock[frequency]);
                                    REQUIRE(after.quantization[destinationFrequency] == before.quantization[frequency]);
                                }
                        }
                }
            }
        }
}

TEST_CASE("Output scan organization is explicit while entropy coding is preserved", "[jpeg][transform][scans]")
{
    for (const bool progressive : {false, true})
        for (const bool arithmetic : {false, true})
            for (const auto requested : {OutputScanOrganization::PreserveSource, OutputScanOrganization::SequentialDct,
                                         OutputScanOrganization::ProgressiveDct})
            {
                CAPTURE(progressive, arithmetic, requested);
                const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, progressive, arithmetic);
                const JpegTransformPlan plan{LosslessTransform::None,
                                             {{48}, {32}},
                                             {{48}, {32}},
                                             EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                             requested,
                                             {{16}, {16}},
                                             {{0}, {0}}};
                std::array<std::byte, 65536> output{};
                const auto transformed =
                    LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
                REQUIRE(transformed.valueIfPresent() != nullptr);
                const auto encoded = std::span(output).first(*transformed.valueIfPresent());
                const auto scanned = JpegSegmentScanner::scan(encoded);
                REQUIRE(scanned.valueIfPresent() != nullptr);
                const bool expectedProgressive = requested == OutputScanOrganization::ProgressiveDct ||
                                                 (requested == OutputScanOrganization::PreserveSource && progressive);
                const auto expected = arithmetic
                                          ? (expectedProgressive ? JpegCodingProcess::ProgressiveDctArithmetic
                                                                 : JpegCodingProcess::ExtendedSequentialDctArithmetic)
                                          : (expectedProgressive ? JpegCodingProcess::ProgressiveDctHuffman
                                                                 : JpegCodingProcess::BaselineDctHuffman);
                CHECK(scanned.valueIfPresent()->frameHeader().codingProcess == expected);
                CHECK(CoefficientDigest::computeSha256Hex(source) == CoefficientDigest::computeSha256Hex(encoded));
            }
}

TEST_CASE("Partial MCU policy rejects perfect rotation and permits explicit trimming", "[jpeg][transform][edges]")
{
    const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, false, false, 31, 19);
    for (const auto edgePolicy :
         {EdgeHandlingPolicy::RequirePerfectCoefficientTransform, EdgeHandlingPolicy::TrimPartialMinimumCodedUnits})
    {
        const JpegTransformPlan plan{LosslessTransform::Rotate90Clockwise,   {{31}, {19}}, {{16}, {31}}, edgePolicy,
                                     OutputScanOrganization::PreserveSource, {{16}, {16}}, {{0}, {3}}};
        std::array<std::byte, 65536> output{};
        const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
        if (edgePolicy == EdgeHandlingPolicy::RequirePerfectCoefficientTransform)
        {
            REQUIRE(result.errorIfPresent() != nullptr);
            CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
        }
        else
        {
            REQUIRE(result.valueIfPresent() != nullptr);
            const auto scanned = JpegSegmentScanner::scan(std::span(output).first(*result.valueIfPresent()));
            REQUIRE(scanned.valueIfPresent() != nullptr);
            CHECK(scanned.valueIfPresent()->frameHeader().dimensions == plan.outputDimensions);
        }
    }
}

TEST_CASE("Every edge policy matches the domain plan on partial horizontal and vertical MCUs",
          "[jpeg][transform][edges]")
{
    for (const auto [width, height] : std::array{std::pair{31, 32}, std::pair{32, 19}, std::pair{31, 19}})
        for (int orientation = 1; orientation <= 8; ++orientation)
            for (const auto policy : {EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                      EdgeHandlingPolicy::TrimPartialMinimumCodedUnits})
            {
                CAPTURE(width, height, orientation, policy);
                const auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr, false, false, width, height);
                const auto plan = JpegTransformPlanner::createPlan(
                    {static_cast<ExifOrientation>(orientation),
                     {{static_cast<std::uint64_t>(width)}, {static_cast<std::uint64_t>(height)}},
                     {{16}, {16}},
                     policy,
                     OutputScanOrganization::PreserveSource});
                if (plan.errorIfPresent())
                {
                    CHECK(plan.errorIfPresent()->code ==
                          ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
                    continue; // The planner itself prohibits dispatch of an impossible perfect transform.
                }
                std::array<std::byte, 65536> output{};
                const auto result =
                    LibJpegTurboCoefficientTransformer::transformCoefficients(source, *plan.valueIfPresent(), output);
                REQUIRE(result.valueIfPresent() != nullptr);
                const auto scanned = JpegSegmentScanner::scan(std::span(output).first(*result.valueIfPresent()));
                REQUIRE(scanned.valueIfPresent() != nullptr);
                CHECK(scanned.valueIfPresent()->frameHeader().dimensions == plan.valueIfPresent()->outputDimensions);
            }
}

TEST_CASE("Marker ownership stays outside TurboJPEG", "[jpeg][transform][markers]")
{
    auto source = createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    // These are namespace-unrecognized payloads, intentionally not metadata
    // parser fixtures. Their exact bytes must not leak into codec output.
    for (const auto marker : std::array<unsigned char, 5>{0xe1, 0xe2, 0xed, 0xef, 0xfe})
    {
        const std::array segment{std::byte{0xff}, std::byte{marker}, std::byte{0},   std::byte{6},
                                 std::byte{'t'},  std::byte{'e'},    std::byte{'s'}, std::byte{'t'}};
        source.insert(source.begin() + 2, segment.begin(), segment.end());
    }
    const auto input = JpegSegmentScanner::scan(source);
    REQUIRE(input.valueIfPresent() != nullptr);
    REQUIRE(input.valueIfPresent()->metadataEncodedLengthBytes() >= 40);
    const JpegTransformPlan plan{LosslessTransform::None,
                                 {{48}, {32}},
                                 {{48}, {32}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    std::array<std::byte, 65536> output{};
    const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto scan = JpegSegmentScanner::scan(std::span(output).first(*result.valueIfPresent()));
    REQUIRE(scan.valueIfPresent() != nullptr);
    for (const auto &marker : scan.valueIfPresent()->markers())
        CHECK((marker.markerCode != 0xe1 && marker.markerCode != 0xe2 && marker.markerCode != 0xed &&
               marker.markerCode != 0xef && marker.markerCode != 0xfe));
}

TEST_CASE("Coefficient adapter preserves an identity transform without copying metadata", "[jpeg][transform]")
{
    const auto source = DeterministicJpegFixtureFactory::createEncodedJpeg(JpegChromaSubsampling::ycbcr420, 1);
    const auto plan = JpegTransformPlanner::createPlan({ExifOrientation::TopLeft,
                                                        {{31}, {19}},
                                                        {{16}, {16}},
                                                        EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                                        OutputScanOrganization::PreserveSource});
    REQUIRE(plan.valueIfPresent() != nullptr);
    std::array<std::byte, 65536> output{};
    const auto result =
        LibJpegTurboCoefficientTransformer::transformCoefficients(source, *plan.valueIfPresent(), output);
    REQUIRE(result.valueIfPresent() != nullptr);
    const auto encodedOutput = std::span(output).first(*result.valueIfPresent());
    CHECK(CoefficientDigest::computeSha256Hex(source) == CoefficientDigest::computeSha256Hex(encodedOutput));
    const auto inventory = JpegSegmentScanner::scan(encodedOutput);
    REQUIRE(inventory.valueIfPresent() != nullptr);
    CHECK(inventory.valueIfPresent()->exifTiffDataRanges().empty());
}

TEST_CASE("Coefficient adapter rejects oversized input before JPEG parsing", "[jpeg][transform][limits]")
{
    // Deliberately not JPEG: the size rejection must win over codec parsing.
    const std::array<std::byte, 5> source{};
    std::array<std::byte, 32> output{};
    output.fill(std::byte{0x5a});
    const JpegResourceLimits limits{4, 268'435'456, 32 * 1024 * 1024, 100};
    const JpegTransformPlan plan{LosslessTransform::None,
                                 {{8}, {8}},
                                 {{8}, {8}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{8}, {8}},
                                 {{0}, {0}}};
    const auto result = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output, {}, limits);
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::EncodedFileTooLarge);
    CHECK(std::ranges::all_of(output, [](auto value) { return value == std::byte{0x5a}; }));
}

TEST_CASE("Cancelled coefficient work leaves the output sink unchanged", "[jpeg][transform]")
{
    std::stop_source cancellation;
    REQUIRE(cancellation.request_stop());
    const std::array<std::byte, 2> source{};
    std::array<std::byte, 32> output{};
    output.fill(std::byte{0x5a});
    const JpegTransformPlan plan{LosslessTransform::None,
                                 {{8}, {8}},
                                 {{8}, {8}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{8}, {8}},
                                 {{0}, {0}}};
    const auto result =
        LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output, cancellation.get_token());
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(result.errorIfPresent()->code == ImageProcessingErrorCode::Cancelled);
    CHECK(std::get<CoefficientTransformationExecutionState>(result.errorIfPresent()->diagnosticContext) ==
          CoefficientTransformationExecutionState::NotStarted);
    CHECK(std::ranges::all_of(output, [](auto value) { return value == std::byte{0x5a}; }));
}
