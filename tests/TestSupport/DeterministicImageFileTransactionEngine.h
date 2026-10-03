#pragma once

#include <jpg_spinner/storage/ImageFileTransactionEngine.h>
#include <atomic>
#include <functional>

namespace jpg_spinner::test_support
{
/// Batch-boundary scheduling adapter, never an approval constructor. Factories
/// receive a real validator capability and may call the native transaction or
/// supply a structured failure. They must support concurrent calls and may use
/// test-owned barriers. Completion is preserved after commit even if cancellation
/// then arrives; unlike pure codec work, committed I/O cannot be undone by a token.
/// Compile its source only in storage-capability test consumers, not the neutral
/// TestSupport library: Domain tests must not acquire a WinRT dependency for it.
class DeterministicImageFileTransactionEngine final : public storage::ImageFileTransactionEngine
{
  public:
    using TransactionFactory = std::function<domain::ImageProcessingResult<storage::CommittedImageFile>(
        const storage::ImageFileTransactionRequest &, const domain::ValidatedJpegOutput &, std::stop_token)>;
    explicit DeterministicImageFileTransactionEngine(TransactionFactory transactionFactory);
    [[nodiscard]] domain::ImageProcessingResult<storage::CommittedImageFile> execute(
        const storage::ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
        std::stop_token cancellationToken = {}) const override;
    [[nodiscard]] unsigned int invocationCount() const noexcept
    {
        return invocations_.load();
    }
    [[nodiscard]] unsigned int maximumConcurrentCalls() const noexcept
    {
        return maximumConcurrentCalls_.load();
    }

  private:
    const TransactionFactory transactionFactory_;
    mutable std::atomic<unsigned int> invocations_{};
    mutable std::atomic<unsigned int> activeCalls_{};
    mutable std::atomic<unsigned int> maximumConcurrentCalls_{};
};
} // namespace jpg_spinner::test_support
