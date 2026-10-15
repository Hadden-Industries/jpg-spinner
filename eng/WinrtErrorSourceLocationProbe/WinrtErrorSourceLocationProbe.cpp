// Exercise actual C++/WinRT error origination and its native WIL-interoperation
// callback, not the projection's private source-location implementation type.
// Release must preserve the HRESULT without embedding caller source locations;
// Debug retains those optional diagnostics. Neither lane creates app UI.
#include <winrt/base.h>

namespace
{
    bool callbackObserved = false;
    bool sourceFileObserved = false;
    std::uint32_t observedLine = 0;
    winrt::hresult observedResult{};
    constexpr winrt::hresult expectedResult{static_cast<std::int32_t>(0x80070005u)};

    void __stdcall CaptureErrorOrigin(std::uint32_t line, char const* file,
        char const*, void*, winrt::hresult result) noexcept
    {
        callbackObserved = true;
        sourceFileObserved = file != nullptr && file[0] != '\0';
        observedLine = line;
        observedResult = result;
    }
}

int main() noexcept
{
    // The callback is the native extension used by WIL to observe originated
    // C++/WinRT errors. It is set before this single-threaded test operation.
    ::winrt_throw_hresult_handler = CaptureErrorOrigin;
    try
    {
        winrt::check_hresult(expectedResult);
    }
    catch (winrt::hresult_error const& error)
    {
        if (!callbackObserved || observedResult != expectedResult || error.code() != expectedResult)
        {
            return 1;
        }
#if defined(_DEBUG)
        return sourceFileObserved && observedLine != 0 ? 0 : 2;
#else
        return !sourceFileObserved && observedLine == 0 ? 0 : 3;
#endif
    }
    catch (...)
    {
        return 4;
    }
    return 5; // The negative HRESULT must really reach the error boundary.
}
