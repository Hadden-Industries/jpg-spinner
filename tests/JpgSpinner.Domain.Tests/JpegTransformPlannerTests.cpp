#include "DomainStringMakers.h"

#include <jpg_spinner/domain/JpegResourceLimits.h>
#include <jpg_spinner/domain/JpegTransformPlanner.h>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <string_view>

namespace
{
using jpg_spinner::domain::DiscardedSourceEdgePixels;
using jpg_spinner::domain::EdgeHandlingPolicy;
using jpg_spinner::domain::ExifOrientation;
using jpg_spinner::domain::ImageDimensions;
using jpg_spinner::domain::ImageProcessingErrorCode;
using jpg_spinner::domain::ImageProcessingStage;
using jpg_spinner::domain::InterleavedMinimumCodedUnitDimensions;
using jpg_spinner::domain::JpegAnalysisFindingCode;
using jpg_spinner::domain::JpegResourceLimit;
using jpg_spinner::domain::JpegResourceLimits;
using jpg_spinner::domain::JpegResourceLimitViolation;
using jpg_spinner::domain::JpegTransformPlanner;
using jpg_spinner::domain::JpegTransformRequest;
using jpg_spinner::domain::LosslessTransform;
using jpg_spinner::domain::OutputScanOrganization;
using jpg_spinner::domain::PixelHeight;
using jpg_spinner::domain::PixelWidth;

constexpr ImageDimensions imageDimensions(const std::uint64_t widthPixels, const std::uint64_t heightPixels) noexcept
{
    return ImageDimensions{PixelWidth{widthPixels}, PixelHeight{heightPixels}};
}

constexpr InterleavedMinimumCodedUnitDimensions interleavedMinimumCodedUnitDimensions(
    const std::uint64_t widthPixels, const std::uint64_t heightPixels) noexcept
{
    return InterleavedMinimumCodedUnitDimensions{
        PixelWidth{widthPixels},
        PixelHeight{heightPixels},
    };
}

constexpr JpegTransformRequest transformRequest(
    const ExifOrientation sourceOrientation, const ImageDimensions sourceDimensions = imageDimensions(32, 24),
    const InterleavedMinimumCodedUnitDimensions minimumCodedUnitDimensions = interleavedMinimumCodedUnitDimensions(8,
                                                                                                                   8),
    const EdgeHandlingPolicy edgeHandlingPolicy = EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
    const OutputScanOrganization outputScanOrganization = OutputScanOrganization::PreserveSource) noexcept
{
    return JpegTransformRequest{
        sourceOrientation, sourceDimensions, minimumCodedUnitDimensions, edgeHandlingPolicy, outputScanOrganization,
    };
}
} // namespace

TEST_CASE("orientation transforms swap exactly the affected image axes", "[domain][planner]")
{
    struct DimensionCase final
    {
        ExifOrientation sourceOrientation;
        ImageDimensions expectedOutputDimensions;
    };

    constexpr std::array cases{
        DimensionCase{ExifOrientation::TopLeft, imageDimensions(32, 24)},
        DimensionCase{ExifOrientation::TopRight, imageDimensions(32, 24)},
        DimensionCase{ExifOrientation::BottomRight, imageDimensions(32, 24)},
        DimensionCase{ExifOrientation::BottomLeft, imageDimensions(32, 24)},
        DimensionCase{ExifOrientation::LeftTop, imageDimensions(24, 32)},
        DimensionCase{ExifOrientation::RightTop, imageDimensions(24, 32)},
        DimensionCase{ExifOrientation::RightBottom, imageDimensions(24, 32)},
        DimensionCase{ExifOrientation::LeftBottom, imageDimensions(24, 32)},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.sourceOrientation);
        const auto result = JpegTransformPlanner::createPlan(transformRequest(testCase.sourceOrientation));

        REQUIRE(result.errorIfPresent() == nullptr);
        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(result.valueIfPresent()->sourceDimensions == imageDimensions(32, 24));
        REQUIRE(result.valueIfPresent()->outputDimensions == testCase.expectedOutputDimensions);
        REQUIRE_FALSE(result.valueIfPresent()->willDiscardEdgePixels());
    }
}

TEST_CASE("sampling-derived iMCU widths govern right-edge perfection", "[domain][planner]")
{
    struct SamplingGeometryCase final
    {
        std::string_view samplingDescription;
        InterleavedMinimumCodedUnitDimensions minimumCodedUnitDimensions;
        std::uint64_t alignedWidthPixels;
    };

    // TurboJPEG 3.2 defines these iMCU geometries for the sampling forms used
    // by JPG Spinner's deterministic 4:4:4, 4:2:2, and 4:2:0 fixtures.
    constexpr std::array cases{
        SamplingGeometryCase{"4:4:4", interleavedMinimumCodedUnitDimensions(8, 8), 8},
        SamplingGeometryCase{"4:2:2", interleavedMinimumCodedUnitDimensions(16, 8), 16},
        SamplingGeometryCase{"4:2:0", interleavedMinimumCodedUnitDimensions(16, 16), 16},
    };

    for (const auto &testCase : cases)
    {
        DYNAMIC_SECTION(testCase.samplingDescription << " aligned width")
        {
            const auto result = JpegTransformPlanner::createPlan(
                transformRequest(ExifOrientation::TopRight, imageDimensions(testCase.alignedWidthPixels, 32),
                                 testCase.minimumCodedUnitDimensions));
            REQUIRE(result.valueIfPresent() != nullptr);
        }

        DYNAMIC_SECTION(testCase.samplingDescription << " partial right iMCU")
        {
            const auto result = JpegTransformPlanner::createPlan(
                transformRequest(ExifOrientation::TopRight, imageDimensions(testCase.alignedWidthPixels - 1, 32),
                                 testCase.minimumCodedUnitDimensions));
            REQUIRE(result.errorIfPresent() != nullptr);
            REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
        }
    }
}

TEST_CASE("perfect-transform availability follows the operation-specific iMCU edges", "[domain][planner]")
{
    struct PerfectTransformCase final
    {
        ExifOrientation sourceOrientation;
        ImageDimensions sourceDimensions;
        bool expectedToBeAvailable;
    };

    constexpr auto iMcu16x16 = interleavedMinimumCodedUnitDimensions(16, 16);
    constexpr std::array cases{
        PerfectTransformCase{ExifOrientation::TopLeft, imageDimensions(31, 19), true},
        PerfectTransformCase{ExifOrientation::TopRight, imageDimensions(31, 32), false},
        PerfectTransformCase{ExifOrientation::TopRight, imageDimensions(32, 19), true},
        PerfectTransformCase{ExifOrientation::BottomRight, imageDimensions(31, 32), false},
        PerfectTransformCase{ExifOrientation::BottomRight, imageDimensions(32, 19), false},
        PerfectTransformCase{ExifOrientation::BottomRight, imageDimensions(32, 32), true},
        PerfectTransformCase{ExifOrientation::BottomLeft, imageDimensions(31, 19), false},
        PerfectTransformCase{ExifOrientation::BottomLeft, imageDimensions(31, 32), true},
        PerfectTransformCase{ExifOrientation::LeftTop, imageDimensions(31, 19), true},
        PerfectTransformCase{ExifOrientation::RightTop, imageDimensions(31, 19), false},
        PerfectTransformCase{ExifOrientation::RightTop, imageDimensions(31, 32), true},
        PerfectTransformCase{ExifOrientation::RightBottom, imageDimensions(31, 32), false},
        PerfectTransformCase{ExifOrientation::RightBottom, imageDimensions(32, 19), false},
        PerfectTransformCase{ExifOrientation::RightBottom, imageDimensions(32, 32), true},
        PerfectTransformCase{ExifOrientation::LeftBottom, imageDimensions(31, 19), false},
        PerfectTransformCase{ExifOrientation::LeftBottom, imageDimensions(32, 19), true},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.sourceOrientation, testCase.sourceDimensions);
        const auto result = JpegTransformPlanner::createPlan(
            transformRequest(testCase.sourceOrientation, testCase.sourceDimensions, iMcu16x16));

        if (testCase.expectedToBeAvailable)
        {
            REQUIRE(result.valueIfPresent() != nullptr);
            REQUIRE(result.valueIfPresent()->isPerfectTransformAvailable());
        }
        else
        {
            REQUIRE(result.errorIfPresent() != nullptr);
            REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
            REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::TransformPlanning);
        }
    }
}

TEST_CASE("explicit trimming reports exact source-edge loss and output dimensions", "[domain][planner]")
{
    struct TrimCase final
    {
        ExifOrientation sourceOrientation;
        LosslessTransform expectedTransform;
        ImageDimensions expectedOutputDimensions;
        DiscardedSourceEdgePixels expectedDiscardedSourceEdgePixels;
    };

    constexpr std::array cases{
        TrimCase{ExifOrientation::TopLeft,
                 LosslessTransform::None,
                 imageDimensions(31, 19),
                 {PixelWidth{0}, PixelHeight{0}}},
        TrimCase{ExifOrientation::TopRight,
                 LosslessTransform::FlipHorizontal,
                 imageDimensions(16, 19),
                 {PixelWidth{15}, PixelHeight{0}}},
        TrimCase{ExifOrientation::BottomRight,
                 LosslessTransform::Rotate180,
                 imageDimensions(16, 16),
                 {PixelWidth{15}, PixelHeight{3}}},
        TrimCase{ExifOrientation::BottomLeft,
                 LosslessTransform::FlipVertical,
                 imageDimensions(31, 16),
                 {PixelWidth{0}, PixelHeight{3}}},
        TrimCase{ExifOrientation::LeftTop,
                 LosslessTransform::Transpose,
                 imageDimensions(19, 31),
                 {PixelWidth{0}, PixelHeight{0}}},
        TrimCase{ExifOrientation::RightTop,
                 LosslessTransform::Rotate90Clockwise,
                 imageDimensions(16, 31),
                 {PixelWidth{0}, PixelHeight{3}}},
        TrimCase{ExifOrientation::RightBottom,
                 LosslessTransform::Transverse,
                 imageDimensions(16, 16),
                 {PixelWidth{15}, PixelHeight{3}}},
        TrimCase{ExifOrientation::LeftBottom,
                 LosslessTransform::Rotate270Clockwise,
                 imageDimensions(19, 16),
                 {PixelWidth{15}, PixelHeight{0}}},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.sourceOrientation);
        const auto result = JpegTransformPlanner::createPlan(transformRequest(
            testCase.sourceOrientation, imageDimensions(31, 19), interleavedMinimumCodedUnitDimensions(16, 16),
            EdgeHandlingPolicy::TrimPartialMinimumCodedUnits));

        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(result.valueIfPresent()->transform == testCase.expectedTransform);
        REQUIRE(result.valueIfPresent()->outputDimensions == testCase.expectedOutputDimensions);
        REQUIRE(result.valueIfPresent()->discardedSourceEdgePixels == testCase.expectedDiscardedSourceEdgePixels);
        REQUIRE(
            result.valueIfPresent()->willDiscardEdgePixels() ==
            (testCase.expectedDiscardedSourceEdgePixels != DiscardedSourceEdgePixels{PixelWidth{0}, PixelHeight{0}}));
    }
}

TEST_CASE("trimming rejects a transform with no complete iMCU on an affected axis", "[domain][planner]")
{
    struct NoRetainedAxisCase final
    {
        ExifOrientation sourceOrientation;
        ImageDimensions sourceDimensions;
    };

    constexpr std::array cases{
        NoRetainedAxisCase{ExifOrientation::TopRight, imageDimensions(15, 32)},
        NoRetainedAxisCase{ExifOrientation::BottomLeft, imageDimensions(32, 15)},
    };

    for (const auto &testCase : cases)
    {
        CAPTURE(testCase.sourceOrientation, testCase.sourceDimensions);
        const auto result = JpegTransformPlanner::createPlan(transformRequest(
            testCase.sourceOrientation, testCase.sourceDimensions, interleavedMinimumCodedUnitDimensions(16, 16),
            EdgeHandlingPolicy::TrimPartialMinimumCodedUnits));

        REQUIRE(result.valueIfPresent() == nullptr);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);
    }
}

TEST_CASE("output scan organization remains an explicit three-state policy", "[domain][planner]")
{
    STATIC_REQUIRE(OutputScanOrganization::PreserveSource != OutputScanOrganization::SequentialDct);
    STATIC_REQUIRE(OutputScanOrganization::PreserveSource != OutputScanOrganization::ProgressiveDct);
    STATIC_REQUIRE(OutputScanOrganization::SequentialDct != OutputScanOrganization::ProgressiveDct);

    for (const auto outputScanOrganization : std::array{
             OutputScanOrganization::PreserveSource,
             OutputScanOrganization::SequentialDct,
             OutputScanOrganization::ProgressiveDct,
         })
    {
        CAPTURE(outputScanOrganization);
        const auto result = JpegTransformPlanner::createPlan(transformRequest(
            ExifOrientation::TopLeft, imageDimensions(32, 24), interleavedMinimumCodedUnitDimensions(8, 8),
            EdgeHandlingPolicy::RequirePerfectCoefficientTransform, outputScanOrganization));

        REQUIRE(result.valueIfPresent() != nullptr);
        REQUIRE(result.valueIfPresent()->outputScanOrganization == outputScanOrganization);
    }
}

TEST_CASE("pixel limits are checked before unsafe multiplication", "[domain][planner]")
{
    constexpr JpegResourceLimits limits{
        512ULL * 1024ULL * 1024ULL,
        100,
        32ULL * 1024ULL * 1024ULL,
        100,
    };

    SECTION("the exact pixel-count limit is accepted")
    {
        const auto result = JpegTransformPlanner::createPlan(
            transformRequest(ExifOrientation::TopLeft, imageDimensions(10, 10)), limits);
        REQUIRE(result.valueIfPresent() != nullptr);
    }

    SECTION("one pixel beyond the limit is rejected with typed context")
    {
        const auto result = JpegTransformPlanner::createPlan(
            transformRequest(ExifOrientation::TopLeft, imageDimensions(101, 1)), limits);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PixelCountLimitExceeded);

        const auto *const violation =
            std::get_if<JpegResourceLimitViolation>(&result.errorIfPresent()->diagnosticContext);
        REQUIRE(violation != nullptr);
        REQUIRE(violation->resourceLimit == JpegResourceLimit::PixelCount);
        REQUIRE(violation->observedValue == 101);
        REQUIRE(violation->maximumValue == 100);
    }

    SECTION("untrusted dimensions whose product would overflow are rejected without a product")
    {
        const auto result = JpegTransformPlanner::createPlan(
            transformRequest(ExifOrientation::TopLeft, imageDimensions(std::numeric_limits<std::uint64_t>::max(), 2)),
            limits);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::PixelCountLimitExceeded);

        const auto *const violation =
            std::get_if<JpegResourceLimitViolation>(&result.errorIfPresent()->diagnosticContext);
        REQUIRE(violation != nullptr);
        REQUIRE_FALSE(violation->observedValue.has_value());
    }
}

TEST_CASE("production JPEG resource limits are explicit policy values", "[domain][planner]")
{
    constexpr auto limits = JpegResourceLimits::production();

    STATIC_REQUIRE(limits.maximumEncodedFileLengthBytes == 512ULL * 1024ULL * 1024ULL);
    STATIC_REQUIRE(limits.maximumPixelCount == 268'435'456ULL);
    STATIC_REQUIRE(limits.maximumMetadataLengthBytes == 32ULL * 1024ULL * 1024ULL);
    STATIC_REQUIRE(limits.maximumProgressiveScanCount == 100U);
}

TEST_CASE("zero JPEG dimensions and iMCU dimensions are rejected before arithmetic", "[domain][planner]")
{
    const auto requireMalformedStructureError = [](const JpegTransformRequest &request) {
        const auto result = JpegTransformPlanner::createPlan(request);

        REQUIRE(result.valueIfPresent() == nullptr);
        REQUIRE(result.errorIfPresent() != nullptr);
        REQUIRE(result.errorIfPresent()->code == ImageProcessingErrorCode::MalformedJpegStructure);
        REQUIRE(result.errorIfPresent()->stage == ImageProcessingStage::TransformPlanning);
    };

    SECTION("zero source width")
    {
        requireMalformedStructureError(transformRequest(ExifOrientation::TopLeft, imageDimensions(0, 1)));
    }

    SECTION("zero source height")
    {
        requireMalformedStructureError(transformRequest(ExifOrientation::TopLeft, imageDimensions(1, 0)));
    }

    SECTION("zero iMCU width")
    {
        requireMalformedStructureError(transformRequest(ExifOrientation::TopLeft, imageDimensions(1, 1),
                                                        interleavedMinimumCodedUnitDimensions(0, 8)));
    }

    SECTION("zero iMCU height")
    {
        requireMalformedStructureError(transformRequest(ExifOrientation::TopLeft, imageDimensions(1, 1),
                                                        interleavedMinimumCodedUnitDimensions(8, 0)));
    }
}

TEST_CASE("planner diagnostics and findings use semantic enum names", "[domain][planner]")
{
    REQUIRE(Catch::StringMaker<EdgeHandlingPolicy>::convert(EdgeHandlingPolicy::RequirePerfectCoefficientTransform) ==
            "RequirePerfectCoefficientTransform");
    REQUIRE(Catch::StringMaker<OutputScanOrganization>::convert(OutputScanOrganization::ProgressiveDct) ==
            "ProgressiveDct");
    REQUIRE(Catch::StringMaker<JpegAnalysisFindingCode>::convert(
                JpegAnalysisFindingCode::PartialMinimumCodedUnitTrimRequired) == "PartialMinimumCodedUnitTrimRequired");
}
