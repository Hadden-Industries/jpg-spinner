#include <string>
#define TXMP_STRING_TYPE std::string
#include <XMP.hpp>
#include <XMP.incl_cpp>
#include "source/UnicodeConversions.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error{message};
}

template <typename Operation> void requireRejection(Operation operation)
{
    bool rejected = false;
    try
    {
        operation();
    }
    catch (const XMP_Error &error)
    {
        rejected = error.GetID() == kXMPErr_BadParam || error.GetID() == kXMPErr_BadValue;
    }
    require(rejected, "An invalid or out-of-range integer was accepted by the native SDK.");
}

void verifyIntegerConversions()
{
    require(SXMPUtils::ConvertToInt(" +42") == 42, "Leading whitespace/sign behaviour changed.");
    require(SXMPUtils::ConvertToInt("2147483647") == (std::numeric_limits<std::int32_t>::max)(), "INT32_MAX changed.");
    require(SXMPUtils::ConvertToInt("-2147483648") == (std::numeric_limits<std::int32_t>::min)(), "INT32_MIN changed.");
    require(SXMPUtils::ConvertToInt("0xffffffff") == -1, "The documented hexadecimal bit pattern changed.");
    require(SXMPUtils::ConvertToInt64("9223372036854775807") == (std::numeric_limits<std::int64_t>::max)(),
            "INT64_MAX changed.");
    require(SXMPUtils::ConvertToInt64("-9223372036854775808") == (std::numeric_limits<std::int64_t>::min)(),
            "INT64_MIN changed.");
    require(SXMPUtils::ConvertToInt64("0xffffffffffffffff") == -1, "The 64-bit hexadecimal bit pattern changed.");
    for (const auto text : {"", "not-a-number", "42suffix", "42 ", "0x", "0X42"})
    {
        requireRejection([&] { SXMPUtils::ConvertToInt(text); });
        requireRejection([&] { SXMPUtils::ConvertToInt64(text); });
    }
}

void verifyUnicodeConversions()
{
    static_assert(sizeof(UTF32Unit) == 4 && sizeof(UTF16Unit) == 2);
    // U+0041, U+20AC and U+1F600 from the Unicode encoding definitions. The
    // supplementary scalar requires a complete UTF-16 surrogate pair.
    constexpr std::array<std::uint8_t, 12> inputBe{0, 0, 0, 0x41, 0, 0, 0x20, 0xAC, 0, 1, 0xF6, 0};
    constexpr std::array<std::uint8_t, 12> inputLe{0x41, 0, 0, 0, 0xAC, 0x20, 0, 0, 0, 0xF6, 1, 0};
    constexpr std::array<std::uint8_t, 8> outputBe{0, 0x41, 0x20, 0xAC, 0xD8, 0x3D, 0xDE, 0};
    constexpr std::array<std::uint8_t, 8> outputLe{0x41, 0, 0xAC, 0x20, 0x3D, 0xD8, 0, 0xDE};
    struct ConversionCase
    {
        const std::array<std::uint8_t, 12> &input;
        const std::array<std::uint8_t, 8> &expected;
        UTF32_to_UTF16_Proc convert;
    };
    const std::array cases{
        ConversionCase{inputBe, outputBe, UTF32BE_to_UTF16BE}, ConversionCase{inputBe, outputLe, UTF32BE_to_UTF16LE},
        ConversionCase{inputLe, outputBe, UTF32LE_to_UTF16BE}, ConversionCase{inputLe, outputLe, UTF32LE_to_UTF16LE}};
    constexpr std::array<std::size_t, 6> expectedRead{0, 1, 2, 2, 3, 3};
    constexpr std::array<std::size_t, 6> expectedWritten{0, 1, 2, 2, 4, 4};
    for (const auto &testCase : cases)
    {
        std::array<UTF32Unit, 3> input;
        std::memcpy(input.data(), testCase.input.data(), testCase.input.size());
        for (std::size_t capacity = 0; capacity != expectedWritten.size(); ++capacity)
        {
            std::array<UTF16Unit, 8> output;
            output.fill(0x5A5A);
            std::size_t read = 99, written = 99;
            testCase.convert(input.data(), input.size(), output.data(), capacity, &read, &written);
            require(read == expectedRead[capacity] && written == expectedWritten[capacity],
                    "Unicode unit counts changed.");
            require(std::memcmp(output.data(), testCase.expected.data(), written * sizeof(UTF16Unit)) == 0,
                    "Unicode bytes changed.");
            for (std::size_t index = written; index != output.size(); ++index)
                require(output[index] == 0x5A5A, "Unicode conversion wrote outside its complete output units.");
        }
    }
}
} // namespace

int main()
{
    if (!SXMPMeta::Initialize())
        return 2;
    int result = 0;
    try
    {
        verifyIntegerConversions();
        verifyUnicodeConversions();
        std::cout << "PASS: valid/malformed integers and all byte-order/buffer-boundary controls.\n";
        for (const auto text : {"2147483648", "-2147483649", "0x100000000"})
            requireRejection([&] { SXMPUtils::ConvertToInt(text); });
        for (const auto text : {"9223372036854775808", "-9223372036854775809", "0x10000000000000000"})
            requireRejection([&] { SXMPUtils::ConvertToInt64(text); });
        std::cout << "PASS: out-of-range integer inputs are rejected.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    catch (const XMP_Error &)
    {
        std::cerr << "An expected valid native conversion raised an XMP error.\n";
        result = 1;
    }
    SXMPMeta::Terminate();
    return result;
}
