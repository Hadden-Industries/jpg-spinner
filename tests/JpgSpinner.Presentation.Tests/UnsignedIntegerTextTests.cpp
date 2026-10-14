#include "pch.h"
#include "Localization/UnsignedIntegerText.h"
#include "PresentationTestApartment.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>

TEST_CASE("Displayed unsigned integers retain locale grouping and all UInt64 digits",
          "[presentation][localization][integer-formatting]")
{
    jpg_spinner::test_support::ensurePresentationTestApartment();
    using winrt::Windows::Globalization::NumberFormatting::DecimalFormatter;
    // Literal externally meaningful examples detect invariant formatting,
    // fractional padding and a lossy intermediate double independently.
    const DecimalFormatter british{{L"en-GB"}, L"GB"};
    const DecimalFormatter american{{L"en-US"}, L"US"};
    const DecimalFormatter russian{{L"ru"}, L"RU"};
    CHECK(jpg_spinner::presentation::formatUnsignedInteger(1234567, british) == L"1,234,567");
    CHECK(jpg_spinner::presentation::formatUnsignedInteger(1234567, american) == L"1,234,567");
    CHECK(jpg_spinner::presentation::formatUnsignedInteger(1234567, russian) == L"1\u00a0234\u00a0567");
    CHECK(jpg_spinner::presentation::formatUnsignedInteger(0, british) == L"0");
    CHECK(jpg_spinner::presentation::formatUnsignedInteger(1, russian) == L"1");
    CHECK(jpg_spinner::presentation::formatUnsignedInteger((std::numeric_limits<std::uint64_t>::max)(), british) ==
          L"18,446,744,073,709,551,615");
}
