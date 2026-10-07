#pragma once

#include <winrt/base.h>
#include <roapi.h>
#include <wil/resource.h>

namespace jpg_spinner::test_support
{
inline void ensurePresentationTestApartment()
{
    // The console driver owns one native WinRT MTA lifetime, like a real host.
    // Per-case COM initialization must not tear down cached WinRT factories
    // between GENERATE cases. WIL balances RoUninitialize at host teardown.
    static auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
    static_cast<void>(apartment);
}
} // namespace jpg_spinner::test_support
