#pragma once

#include <cstdint>
#include <optional>

namespace jpg_spinner::batch_processing
{
/// Scheduling policy, independent of the per-image JPEG acceptance limits.
/// The byte budget admits estimated overlapping work; it is not an OS process
/// memory quota. An oversized image runs alone under the existing codec limits.
struct BatchExecutionLimits final
{
    static constexpr std::uint32_t maximumSupportedConcurrentImageCount = 64;
    static constexpr std::uint64_t bytesPerMebibyte = 1024ULL * 1024;
    std::uint32_t maximumConcurrentImageCount{1};
    /// Absence selects Automatic; an explicit budget can only reduce the
    /// system-derived admission budget, never grant extra memory authority.
    std::optional<std::uint64_t> estimatedWorkingMemoryBudgetBytes{};
    [[nodiscard]] static std::uint32_t deviceConcurrentImageLimit() noexcept;
    [[nodiscard]] static std::uint64_t recommendedWorkingMemoryBudgetBytes() noexcept;
    [[nodiscard]] static BatchExecutionLimits recommended() noexcept;
};
} // namespace jpg_spinner::batch_processing
