#define NOMINMAX
#include <jpg_spinner/batch/BatchIdentifier.h>
#include <objbase.h>
#include <format>
#include <system_error>

#pragma comment(lib, "ole32.lib")

namespace jpg_spinner::batch_processing
{
BatchIdentifier::BatchIdentifier(const GUID fullIdentity, const std::chrono::sys_seconds creationTime) noexcept
    : fullIdentity_(fullIdentity), creationTime_(creationTime)
{
}

BatchIdentifier BatchIdentifier::create(const std::chrono::sys_seconds creationTime)
{
    GUID identity{};
    const auto result = CoCreateGuid(&identity);
    if (FAILED(result))
        throw std::system_error(static_cast<int>(result), std::system_category(), "Batch UUID generation failed");
    return {identity, creationTime};
}

const GUID &BatchIdentifier::fullIdentity() const noexcept
{
    return fullIdentity_;
}

std::chrono::sys_seconds BatchIdentifier::creationTime() const noexcept
{
    return creationTime_;
}

std::wstring BatchIdentifier::displayDirectoryName() const
{
    // sys_seconds is UTC; omitting the locale-aware format modifier makes the
    // directory label invariant. GUID Data1 is the canonical first eight hex
    // digits, not the first eight bytes of the platform's in-memory GUID layout.
    return std::format(L"{:%Y%m%dT%H%M%SZ}-{:08x}", creationTime_, fullIdentity_.Data1);
}
} // namespace jpg_spinner::batch_processing
