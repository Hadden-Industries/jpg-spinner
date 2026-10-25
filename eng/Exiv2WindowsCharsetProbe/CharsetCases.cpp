// Exercise the public dependency API without a mocked converter or filesystem.
#include "WindowsCharsetConversionCases.h"
#include <exiv2/convert.hpp>
#include <iostream>

int main()
{
    const auto cases = jpg_spinner::test_support::windowsCharsetConversionCases();
    for (const auto &testCase : cases)
    {
        auto observedBytes = testCase.inputBytes;
        const auto success =
            Exiv2::convertStringCharset(observedBytes, testCase.sourceEncoding, testCase.destinationEncoding);
        if (success != testCase.expectedSuccess || observedBytes != testCase.expectedBytes)
        {
            std::cerr << "FAIL: " << testCase.name << '\n';
            return 1;
        }
    }
    std::cout << "PASS: " << cases.size() << " actual Exiv2 Windows charset cases.\n";
}
