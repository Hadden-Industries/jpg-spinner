#include "PresentationTestApartment.h"
#include "Imaging/ProgressiveJpegExample.h"
#include <Windows.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <filesystem>

TEST_CASE("The original progressive example decodes three distinct native levels",
          "[presentation][progressive-example]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    std::array<wchar_t, 32768> executable{};
    const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    REQUIRE(length != 0);
    REQUIRE(length < executable.size());
    const auto example = std::filesystem::path{executable.data()}.parent_path().parent_path() / L"JpgSpinner.App" /
                         L"Assets" / L"Help" / L"ProgressiveEncodingExample.jpg";
    const auto frames = jpg_spinner::presentation::decodeProgressiveJpegExample(example);
    for (const auto &frame : frames)
    {
        CHECK(frame.widthInPixels == 900);
        CHECK(frame.heightInPixels == 600);
        CHECK(frame.premultipliedBgraPixels.size() == 900 * 600 * 4);
        CHECK(frame.progressiveLevel < frame.progressiveLevelCount);
    }
    CHECK(frames.front().progressiveLevel == 0);
    CHECK(frames[1].progressiveLevel > frames.front().progressiveLevel);
    CHECK(frames.back().progressiveLevel > frames[1].progressiveLevel);
    CHECK(frames.back().progressiveLevel + 1 == frames.back().progressiveLevelCount);
    // This catches a converter caching the final frame for every level, which
    // would make a visually populated carousel teach the wrong behaviour.
    CHECK(frames.front().premultipliedBgraPixels != frames[1].premultipliedBgraPixels);
    CHECK(frames[1].premultipliedBgraPixels != frames.back().premultipliedBgraPixels);
}
