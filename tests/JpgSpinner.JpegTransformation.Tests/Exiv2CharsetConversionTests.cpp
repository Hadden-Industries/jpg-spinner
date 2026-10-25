#include "WindowsCharsetConversionCases.h"
#include <catch2/catch_test_macros.hpp>
#include <exiv2/convert.hpp>

TEST_CASE("Exiv2 preserves the Windows charset conversion contract", "[metadata][exiv2][charset]")
{
    // The normal product-linked suite consumes the same independent byte cases
    // as the source qualification probe, including failures that preserve input.
    for (const auto &testCase : jpg_spinner::test_support::windowsCharsetConversionCases())
    {
        DYNAMIC_SECTION(testCase.name)
        {
            auto observedBytes = testCase.inputBytes;
            REQUIRE(Exiv2::convertStringCharset(observedBytes, testCase.sourceEncoding, testCase.destinationEncoding) ==
                    testCase.expectedSuccess);
            REQUIRE(observedBytes == testCase.expectedBytes);
        }
    }
}
