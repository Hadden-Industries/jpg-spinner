#include "internal/TurboJpegResourceLimits.h"
#include "internal/LibJpegTurboCoefficientTransformer.h"
#include "CoefficientTransformFixture.h"

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <memory>
#include <turbojpeg.h>

using jpg_spinner::jpeg::internal::TurboJpegResourceLimits;

TEST_CASE("A zero intermediate budget cannot silently disable the bound", "[jpeg][transform][limits]")
{
    using namespace jpg_spinner::domain;
    const auto source = jpg_spinner::test_support::createTransformFixture(8, TJSAMP_420, TJCS_YCbCr);
    const JpegTransformPlan plan{LosslessTransform::None,
                                 {{48}, {32}},
                                 {{48}, {32}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{16}, {16}},
                                 {{0}, {0}}};
    std::vector<std::byte> output(65536, std::byte{0x5a});
    const auto result = jpg_spinner::jpeg::internal::LibJpegTurboCoefficientTransformer::transformCoefficients(
        source, plan, output, {}, JpegResourceLimits::production(), TurboJpegResourceLimits{0});
    REQUIRE(result.errorIfPresent() != nullptr);
    CHECK(std::ranges::all_of(output, [](auto value) { return value == std::byte{0x5a}; }));
}

TEST_CASE("The adapter applies its intermediate memory budget to the native coefficient operation",
          "[jpeg][transform][limits]")
{
    using namespace jpg_spinner::domain;
    using jpg_spinner::jpeg::internal::LibJpegTurboCoefficientTransformer;
    const auto source =
        jpg_spinner::test_support::createTransformFixture(8, TJSAMP_444, TJCS_YCbCr, false, false, 512, 512);
    const JpegTransformPlan plan{LosslessTransform::Transpose,
                                 {{512}, {512}},
                                 {{512}, {512}},
                                 EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
                                 OutputScanOrganization::PreserveSource,
                                 {{8}, {8}},
                                 {{0}, {0}}};
    std::vector<std::byte> output(4 * 1024 * 1024, std::byte{0x5a});
    const auto rejected = LibJpegTurboCoefficientTransformer::transformCoefficients(
        source, plan, output, {}, JpegResourceLimits::production(), TurboJpegResourceLimits{1});
    REQUIRE(rejected.errorIfPresent() != nullptr);
    CHECK(rejected.errorIfPresent()->code == ImageProcessingErrorCode::CoefficientTransformationFailed);
    CHECK(std::ranges::all_of(output, [](auto value) { return value == std::byte{0x5a}; }));
    // Positive control: the same real fixture is supported with the production
    // budget. No mock stands in for TurboJPEG's coefficient allocation.
    const auto accepted = LibJpegTurboCoefficientTransformer::transformCoefficients(source, plan, output);
    REQUIRE(accepted.valueIfPresent() != nullptr);
}

TEST_CASE("TurboJPEG intermediate memory uses mebibytes independently of encoded input size",
          "[jpeg][transform][limits]")
{
    // The pinned upstream implementation multiplies MAXMEMORY by 1048576,
    // despite the API documentation's ambiguous word 'megabytes'. This is a
    // native parameter contract test, not a claim about whole-process RSS.
    constexpr auto limits = TurboJpegResourceLimits::production();
    STATIC_REQUIRE(limits.maximumIntermediateBufferMemoryMebibytes == 512);
    STATIC_REQUIRE(static_cast<std::uint64_t>(limits.maximumIntermediateBufferMemoryMebibytes) * 1'048'576 ==
                   536'870'912);
    const std::unique_ptr<void, decltype(&tj3Destroy)> context(tj3Init(TJINIT_TRANSFORM), &tj3Destroy);
    REQUIRE(context != nullptr);
    REQUIRE(tj3Set(context.get(), TJPARAM_MAXMEMORY, limits.maximumIntermediateBufferMemoryMebibytes) == 0);
    REQUIRE(tj3Get(context.get(), TJPARAM_MAXMEMORY) == 512);
}
