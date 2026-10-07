#define NOMINMAX
#include <jpg_spinner/batch/BatchIdentifier.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <chrono>
#include <objbase.h>

using jpg_spinner::batch_processing::BatchIdentifier;
using namespace std::chrono;

TEST_CASE("Batch display names use UTC seconds and only abbreviate the full identity", "[batch][identifier]")
{
    const GUID firstIdentity{0xabcdef01, 0x1234, 0x4321, {0x80, 1, 2, 3, 4, 5, 6, 7}};
    const GUID secondIdentity{0xabcdef01, 0x1234, 0x4321, {0x80, 1, 2, 3, 4, 5, 6, 8}};
    const auto creationTime = sys_days{year{2026} / January / 2} + 3h + 4min + 5s;
    const BatchIdentifier first{firstIdentity, creationTime};
    const BatchIdentifier second{secondIdentity, creationTime};

    REQUIRE(first.displayDirectoryName() == L"20260102T030405Z-abcdef01");
    REQUIRE(second.displayDirectoryName() == first.displayDirectoryName());
    REQUIRE(IsEqualGUID(first.fullIdentity(), firstIdentity));
    REQUIRE(IsEqualGUID(second.fullIdentity(), secondIdentity));
    REQUIRE_FALSE(IsEqualGUID(first.fullIdentity(), second.fullIdentity()));
    REQUIRE(first.creationTime() == creationTime);
}

TEST_CASE("Generated batches retain complete native identifiers", "[batch][identifier]")
{
    const auto creationTime = sys_seconds{seconds{0}};
    const auto first = BatchIdentifier::create(creationTime);
    const auto second = BatchIdentifier::create(creationTime);
    REQUIRE_FALSE(IsEqualGUID(first.fullIdentity(), second.fullIdentity()));
    REQUIRE_FALSE(IsEqualGUID(first.fullIdentity(), GUID{}));
    std::array<wchar_t, 39> rendered{};
    REQUIRE(StringFromGUID2(first.fullIdentity(), rendered.data(), static_cast<int>(rendered.size())) == 39);
    REQUIRE(first.displayDirectoryName().substr(0, 17) == L"19700101T000000Z-");
}
