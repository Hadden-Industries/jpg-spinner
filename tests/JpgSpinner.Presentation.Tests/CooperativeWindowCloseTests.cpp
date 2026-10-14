#include "Composition/CooperativeWindowClose.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Window close waits for active work, not just a cancellation request", "[presentation][shutdown]")
{
    jpg_spinner::presentation::CooperativeWindowClose close;
    CHECK_FALSE(close.closeRequested());
    CHECK_FALSE(close.shouldCloseAfterWorkSettles(false));
    CHECK_FALSE(close.requestClose(true));
    CHECK(close.closeRequested());
    CHECK_FALSE(close.shouldCloseAfterWorkSettles(true));
    CHECK_FALSE(close.requestClose(true)); // Repeated X/Alt-F4 never skips the boundary.
    CHECK(close.shouldCloseAfterWorkSettles(false));
}

TEST_CASE("An idle window closes immediately without admitting new work", "[presentation][shutdown]")
{
    jpg_spinner::presentation::CooperativeWindowClose close;
    CHECK(close.requestClose(false));
    CHECK(close.closeRequested());
    CHECK(close.shouldCloseAfterWorkSettles(false));
}
