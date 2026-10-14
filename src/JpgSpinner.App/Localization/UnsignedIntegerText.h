#pragma once
#include <cstdint>
#include <winrt/Windows.Globalization.NumberFormatting.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace jpg_spinner::presentation
{
/// Display an exact unsigned integer using the supplied native locale formatter.
/// The formatter is caller-owned and must not be shared concurrently; configure
/// grouping and whole-number display here, but let Windows own cultural rules.
inline winrt::hstring formatUnsignedInteger(
    std::uint64_t value, winrt::Windows::Globalization::NumberFormatting::DecimalFormatter const &formatter)
{
    // Counts/extents have no fractional part. Use the integer entry point so
    // values beyond 2^53 never lose precision through a Double conversion.
    // https://learn.microsoft.com/uwp/api/windows.globalization.numberformatting.decimalformatter.formatuint
    formatter.FractionDigits(0);
    formatter.IntegerDigits(1);
    formatter.IsGrouped(true);
    formatter.IsDecimalPointAlwaysDisplayed(false);
    return formatter.FormatUInt(value);
}

/// Use Windows' application runtime languages/region, not invariant conversion.
/// Native activation/formatting errors propagate; no ASCII fallback hides them.
inline winrt::hstring formatUnsignedInteger(std::uint64_t value)
{
    return formatUnsignedInteger(value, winrt::Windows::Globalization::NumberFormatting::DecimalFormatter{});
}
} // namespace jpg_spinner::presentation
