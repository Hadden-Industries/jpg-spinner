#pragma once

#include <winrt/base.h>

namespace jpg_spinner::test_support
{
/// Owns one successful native initialization count on the calling thread. A
/// pre-existing MTA is balanced too; this object must not move across threads.
class MultithreadedApartmentLifetime final
{
  public:
    MultithreadedApartmentLifetime()
    {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
    }
    ~MultithreadedApartmentLifetime()
    {
        winrt::uninit_apartment();
    }
    MultithreadedApartmentLifetime(const MultithreadedApartmentLifetime &) = delete;
    MultithreadedApartmentLifetime &operator=(const MultithreadedApartmentLifetime &) = delete;
};
} // namespace jpg_spinner::test_support
