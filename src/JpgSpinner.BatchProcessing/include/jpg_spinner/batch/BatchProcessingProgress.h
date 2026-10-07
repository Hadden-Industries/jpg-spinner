#pragma once

#include <cstddef>
#include <functional>

namespace jpg_spinner::batch_processing
{
/// Candidate counts, not estimates. analyzed counts completed analyzer calls
/// including unsupported inputs; eligible counts successful non-identity plans.
/// completed counts terminal file results, including failed/skipped/cancelled.
/// remaining = discovered - completed. The final denominator is unknown until
/// discoveryComplete; partial cancellation never pretends discovery finished.
struct BatchProcessingProgress final
{
    std::size_t discovered{};
    std::size_t analyzed{};
    std::size_t eligible{};
    std::size_t completed{};
    std::size_t failed{};
    std::size_t skipped{};
    std::size_t cancelled{};
    std::size_t remaining{};
    bool discoveryComplete{false};
};
/// Synchronous background-worker observation; observers must not throw or block.
using BatchProcessingProgressObserver = std::function<void(const BatchProcessingProgress &)>;
} // namespace jpg_spinner::batch_processing
