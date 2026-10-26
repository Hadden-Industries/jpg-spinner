#include <exiv2/exv_conf.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("The metadata dependency cannot perform path-based filesystem operations",
          "[metadata][dependency-capabilities]")
{
    // This checks the installed dependency's generated native configuration,
    // not the spelling of our CMake arguments. The application deliberately
    // delegates file authority to Windows Storage and passes isolated payloads
    // to Exiv2; accidentally restoring FileIo reintroduces another write path.
#ifdef EXV_ENABLE_FILESYSTEM
    FAIL("Exiv2 filesystem access is enabled in the selected native dependency.");
#else
    SUCCEED("Exiv2 is built for memory-backed metadata APIs only.");
#endif
}
