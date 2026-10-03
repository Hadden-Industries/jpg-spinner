#include "DeterministicImageFileTransactionEngine.h"
#include <stdexcept>
#include <utility>

namespace jpg_spinner::test_support
{
DeterministicImageFileTransactionEngine::DeterministicImageFileTransactionEngine(TransactionFactory transactionFactory)
    : transactionFactory_(std::move(transactionFactory))
{
    if (!transactionFactory_)
        throw std::invalid_argument("A deterministic transaction factory is required.");
}

domain::ImageProcessingResult<storage::CommittedImageFile> DeterministicImageFileTransactionEngine::execute(
    const storage::ImageFileTransactionRequest &request, const domain::ValidatedJpegOutput &validatedOutput,
    const std::stop_token cancellationToken) const
{
    ++invocations_;
    if (cancellationToken.stop_requested())
        return domain::ImageProcessingResult<storage::CommittedImageFile>::failure(
            {domain::ImageProcessingErrorCode::Cancelled, domain::ImageProcessingStage::StagingFileCreation});
    const auto active = ++activeCalls_;
    auto maximum = maximumConcurrentCalls_.load();
    while (maximum < active && !maximumConcurrentCalls_.compare_exchange_weak(maximum, active))
    {
    }
    struct ActiveCall final
    {
        std::atomic<unsigned int> &count;
        ~ActiveCall()
        {
            --count;
        }
    } activeCall{activeCalls_};
    // A factory exception is a fixture defect, not a fabricated domain error.
    // Do not apply a post-call cancellation override to already completed I/O.
    return transactionFactory_(request, validatedOutput, cancellationToken);
}
domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary> DeterministicImageFileTransactionEngine::
    recoverIncompleteTransactions(std::stop_token cancellationToken) const
{
    return domain::ImageProcessingResult<storage::ImageFileTransactionRecoverySummary>::failure(
        {cancellationToken.stop_requested() ? domain::ImageProcessingErrorCode::Cancelled
                                            : domain::ImageProcessingErrorCode::RecoveryConflict,
         domain::ImageProcessingStage::TransactionRecovery});
}
} // namespace jpg_spinner::test_support
