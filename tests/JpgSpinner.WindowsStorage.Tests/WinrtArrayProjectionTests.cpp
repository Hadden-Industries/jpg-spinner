#include "MultithreadedApartmentLifetime.h"
#include <catch2/catch_test_macros.hpp>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Security.Cryptography.h>
#include <winrt/Windows.Storage.Streams.h>
#include <algorithm>
#include <cstdint>
#include <vector>

TEST_CASE("The selected WinRT projection preserves native receive-array lengths and byte contents",
          "[storage][dependency-abi]")
{
    jpg_spinner::test_support::MultithreadedApartmentLifetime apartment;
    // Characterize the public API that instantiates Microsoft's generated
    // array-size proxy. Do not patch that generated ABI helper or duplicate its
    // object-layout arithmetic to make an analyzer diagnostic disappear.
    for (const auto &expected :
         {std::vector<std::uint8_t>{}, std::vector<std::uint8_t>{0}, std::vector<std::uint8_t>{0x17, 0, 0xFF, 0x80}})
    {
        const auto buffer = winrt::Windows::Security::Cryptography::CryptographicBuffer::CreateFromByteArray(expected);
        winrt::com_array<std::uint8_t> actual(17, 0xA5);
        winrt::Windows::Security::Cryptography::CryptographicBuffer::CopyToByteArray(buffer, actual);
        REQUIRE(actual.size() == expected.size());
        CHECK(std::equal(expected.begin(), expected.end(), actual.begin()));
    }
}

TEST_CASE("The selected WinRT projection receives native interface identifiers without corrupting the boxed value",
          "[storage][dependency-abi]")
{
    jpg_spinner::test_support::MultithreadedApartmentLifetime apartment;
    const auto value = winrt::box_value(std::int32_t{42});
    // get_interfaces exercises the GUID-array instantiation reported separately
    // from byte arrays. Its output is supplied by the real Windows Runtime.
    const auto identifiers = winrt::get_interfaces(value);
    REQUIRE_FALSE(identifiers.empty());
    const auto expected = winrt::guid_of<winrt::Windows::Foundation::IReference<std::int32_t>>();
    CHECK(std::find(identifiers.begin(), identifiers.end(), expected) != identifiers.end());
    CHECK(winrt::unbox_value<std::int32_t>(value) == 42);
}
