#define NOMINMAX
#include <windows.h>
#include <jpg_spinner/batch/BatchExecutionLimits.h>
#include <algorithm>
#include <thread>

namespace jpg_spinner::batch_processing
{
std::uint32_t BatchExecutionLimits::deviceConcurrentImageLimit() noexcept
{
    // hardware_concurrency is a standard-library hint; zero means unknown.
    return std::clamp(std::thread::hardware_concurrency(), 1U, maximumSupportedConcurrentImageCount);
}
std::uint64_t BatchExecutionLimits::recommendedWorkingMemoryBudgetBytes() noexcept
{
    MEMORYSTATUSEX memory{sizeof(memory)};
    if (!GlobalMemoryStatusEx(&memory))
        return 1; // Unknown pressure admits one image, never an unbounded pool.
    // Leave physical and commit headroom for the UI and other applications.
    // This admission estimate does not reserve RAM or impose an OS hard quota.
    return std::max<std::uint64_t>(
        1, std::min({memory.ullTotalPhys / 4, memory.ullAvailPhys / 2, memory.ullAvailPageFile / 2}));
}
BatchExecutionLimits BatchExecutionLimits::recommended() noexcept
{
    return {std::min(4U, deviceConcurrentImageLimit()), recommendedWorkingMemoryBudgetBytes()};
}
} // namespace jpg_spinner::batch_processing
