#pragma once

#include <jpg_spinner/jpeg/JpegTransformationEngine.h>
#include <atomic>
#include <functional>

namespace jpg_spinner::test_support
{
/// Batch-boundary test adapter. A supplied deterministic factory returns a
/// structured failure or a capability obtained through real JPEG validation;
/// it cannot forge ValidatedJpegOutput. The factory may use test-owned barriers
/// to control completion and must support concurrent calls. Observations record
/// public invocation concurrency, never internal production phase calls.
/// A successful factory represents completed coefficient work; errors retain
/// the factory's original native-admission diagnostic even after a stop request.
class DeterministicJpegTransformationEngine final : public jpg_spinner::jpeg::JpegTransformationEngine
{
  public:
    using OutputFactory =
        std::function<jpg_spinner::domain::ImageProcessingResult<jpg_spinner::domain::ValidatedJpegOutput>(
            std::vector<std::byte>, jpg_spinner::domain::JpegImageAnalysis, std::stop_token)>;
    explicit DeterministicJpegTransformationEngine(OutputFactory outputFactory);
    [[nodiscard]] jpg_spinner::domain::ImageProcessingResult<jpg_spinner::domain::ValidatedJpegOutput>
    createValidatedOutput(std::vector<std::byte> source, jpg_spinner::domain::JpegImageAnalysis approvedAnalysis,
                          std::stop_token cancellation = {}) const override;
    [[nodiscard]] unsigned int invocationCount() const noexcept
    {
        return invocations_.load();
    }
    [[nodiscard]] unsigned int maximumConcurrentCalls() const noexcept
    {
        return maximumConcurrentCalls_.load();
    }

  private:
    const OutputFactory outputFactory_;
    mutable std::atomic<unsigned int> invocations_{};
    mutable std::atomic<unsigned int> activeCalls_{};
    mutable std::atomic<unsigned int> maximumConcurrentCalls_{};
};
} // namespace jpg_spinner::test_support
