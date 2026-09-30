#include <cstddef>
#include <cstdint>

// Proves the installed fuzzer supplies its entry point and can execute an
// ASan-instrumented callback. The seeded overflow is not retained in this target.
#if !defined(__SANITIZE_ADDRESS__)
#error The fuzz toolchain smoke requires AddressSanitizer instrumentation.
#endif
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, const std::size_t size)
{
    if (size != 0)
    {
        volatile auto lastInputByte = data[size - 1];
        (void)lastInputByte;
    }
    return 0;
}
