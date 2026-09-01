#include "DomainStringMakers.h"

#include <jpg_spinner/domain/ExifOrientation.h>
#include <jpg_spinner/domain/LosslessTransform.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <array>
#include <cstdint>
#include <optional>

namespace
{
using jpg_spinner::domain::ExifOrientation;
using jpg_spinner::domain::LosslessTransform;
using jpg_spinner::domain::losslessTransformFor;
using jpg_spinner::domain::tryParseExifOrientation;

struct OrientationTransformCase final
{
    ExifOrientation sourceOrientation;
    LosslessTransform expectedTransform;
};
} // namespace

TEST_CASE("every Exif orientation maps to the coefficient transform that produces TopLeft", "[domain][orientation]")
{
    constexpr std::array cases{
        OrientationTransformCase{ExifOrientation::TopLeft, LosslessTransform::None},
        OrientationTransformCase{ExifOrientation::TopRight, LosslessTransform::FlipHorizontal},
        OrientationTransformCase{ExifOrientation::BottomRight, LosslessTransform::Rotate180},
        OrientationTransformCase{ExifOrientation::BottomLeft, LosslessTransform::FlipVertical},
        OrientationTransformCase{ExifOrientation::LeftTop, LosslessTransform::Transpose},
        OrientationTransformCase{ExifOrientation::RightTop, LosslessTransform::Rotate90Clockwise},
        OrientationTransformCase{ExifOrientation::RightBottom, LosslessTransform::Transverse},
        OrientationTransformCase{ExifOrientation::LeftBottom, LosslessTransform::Rotate270Clockwise},
    };

    const auto &testCase = GENERATE_REF(Catch::Generators::from_range(cases));

    DYNAMIC_SECTION("source " << Catch::StringMaker<ExifOrientation>::convert(testCase.sourceOrientation))
    {
        REQUIRE(losslessTransformFor(testCase.sourceOrientation) == testCase.expectedTransform);
    }
}

TEST_CASE("only the eight Exif Orientation tag values are accepted", "[domain][orientation]")
{
    constexpr std::array validOrientations{
        ExifOrientation::TopLeft, ExifOrientation::TopRight, ExifOrientation::BottomRight, ExifOrientation::BottomLeft,
        ExifOrientation::LeftTop, ExifOrientation::RightTop, ExifOrientation::RightBottom, ExifOrientation::LeftBottom,
    };

    for (std::uint16_t rawOrientation = 1; rawOrientation <= validOrientations.size(); ++rawOrientation)
    {
        CAPTURE(rawOrientation);
        REQUIRE(tryParseExifOrientation(rawOrientation) == validOrientations[rawOrientation - 1]);
    }

    for (const std::uint16_t invalidRawOrientation : std::array<std::uint16_t, 3>{0, 9, 255})
    {
        CAPTURE(invalidRawOrientation);
        REQUIRE(tryParseExifOrientation(invalidRawOrientation) == std::nullopt);
    }
}

TEST_CASE("orientation diagnostics use semantic enumerator names", "[domain][orientation]")
{
    REQUIRE(Catch::StringMaker<ExifOrientation>::convert(ExifOrientation::RightBottom) == "RightBottom");
    REQUIRE(Catch::StringMaker<LosslessTransform>::convert(LosslessTransform::Rotate270Clockwise) ==
            "Rotate270Clockwise");
}
