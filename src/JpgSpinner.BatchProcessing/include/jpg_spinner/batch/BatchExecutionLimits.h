#pragma once

#include <cstdint>

namespace jpg_spinner::batch_processing
{
/// Scheduling policy, independent of the per-image JPEG acceptance limits.
/// The byte budget admits estimated overlapping work; it is not an OS process
/// memory quota. An oversized image runs alone under the existing codec limits.
struct BatchExecutionLimits final
{
    static constexpr std::uint32_t maximumSupportedConcurrentImageCount = 64;
    std::uint32_t maximumConcurrentImageCount{1};
    std::uint64_t estimatedWorkingMemoryBudgetBytes{2ULL * 1024 * 1024 * 1024};
    [[nodiscard]] static std::uint32_t deviceConcurrentImageLimit() noexcept;
    [[nodiscard]] static std::uint64_t recommendedWorkingMemoryBudgetBytes() noexcept;
    [[nodiscard]] static BatchExecutionLimits recommended() noexcept;
};
} // namespace jpg_spinner::batch_processing
