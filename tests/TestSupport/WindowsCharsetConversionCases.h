// Native Windows byte fixtures shared by component qualification and the
// ordinary Catch2 suite. Keep one independent catalog for both consumers.
#pragma once
#include <array>
#include <string>

namespace jpg_spinner::test_support
{
struct EncodingConversionCase
{
    const char *name;
    const char *sourceEncoding;
    const char *destinationEncoding;
    std::string inputBytes;
    std::string expectedBytes;
    bool expectedSuccess{true};
};

inline auto windowsCharsetConversionCases()
{
    using namespace std::string_literals;
    return std::array{
        EncodingConversionCase{"empty UTF-8", "UTF-8", "UCS-2LE", {}, {}},
        EncodingConversionCase{"empty UTF-16", "UCS-2LE", "UTF-8", {}, {}},
        EncodingConversionCase{"BMP little-endian", "UTF-8", "UCS-2LE", "\xc3\xa9", "\xe9\0"s},
        EncodingConversionCase{"BMP big-endian", "UTF-8", "UCS-2BE", "\xc3\xa9", "\0\xe9"s},
        EncodingConversionCase{"little-endian to UTF-8", "UCS-2LE", "UTF-8", "\xe9\0"s, "\xc3\xa9"},
        EncodingConversionCase{"big-endian to UTF-8", "UCS-2BE", "UTF-8", "\0\xe9"s, "\xc3\xa9"},
        EncodingConversionCase{"embedded NUL forward", "UTF-8", "UCS-2LE", "A\0B"s, "A\0\0\0B\0"s},
        EncodingConversionCase{"embedded NUL reverse", "UCS-2LE", "UTF-8", "A\0\0\0B\0"s, "A\0B"s},
        // Preserve actual Windows UTF-16 behavior despite the legacy API's
        // UCS-2 spelling. A surrogate pair must not be truncated or renamed.
        EncodingConversionCase{"supplementary forward", "UTF-8", "UCS-2LE", "\xf0\x9f\x98\x80", "\x3d\xd8\0\xde"s},
        EncodingConversionCase{"supplementary reverse", "UCS-2LE", "UTF-8", "\x3d\xd8\0\xde"s, "\xf0\x9f\x98\x80"},
        EncodingConversionCase{"big-endian supplementary", "UCS-2BE", "UTF-8", "\xd8\x3d\xde\0"s, "\xf0\x9f\x98\x80"},
        EncodingConversionCase{"little-to-big endian", "UCS-2LE", "UCS-2BE", "A\0\xe9\0"s, "\0A\0\xe9"s},
        EncodingConversionCase{"big-to-little endian", "UCS-2BE", "UCS-2LE", "\0A\0\xe9"s, "A\0\xe9\0"s},
        EncodingConversionCase{"Latin-1 with NUL", "ISO-8859-1", "UTF-8", "\xe9\0\xff"s, "\xc3\xa9\0\xc3\xbf"s},
        EncodingConversionCase{"ASCII", "ASCII", "UTF-8", "metadata", "metadata"},
        EncodingConversionCase{"odd little-endian refusal", "UCS-2LE", "UTF-8", "A", "A", false},
        EncodingConversionCase{"odd big-endian refusal", "UCS-2BE", "UTF-8", "A", "A", false},
        EncodingConversionCase{"unsupported encoding", "UTF-32", "UTF-8", "unchanged", "unchanged", false},
        // Flags zero intentionally retain Win32's documented replacement policy.
        EncodingConversionCase{"invalid UTF-8 replacement", "UTF-8", "UCS-2LE", "\xff", "\xfd\xff"s},
        EncodingConversionCase{"unpaired surrogate replacement", "UCS-2LE", "UTF-8", "\0\xd8"s, "\xef\xbf\xbd"},
    };
}
} // namespace jpg_spinner::test_support
