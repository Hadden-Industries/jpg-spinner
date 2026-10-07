#include "DeterministicJpegTransformationEngine.h"
#include <stdexcept>

namespace jpg_spinner::test_support
{
using namespace jpg_spinner::domain;
DeterministicJpegTransformationEngine::DeterministicJpegTransformationEngine(OutputFactory outputFactory)
    : outputFactory_(std::move(outputFactory))
{
    if (!outputFactory_)
        throw std::invalid_argument("A deterministic output factory is required.");
}
ImageProcessingResult<ValidatedJpegOutput> DeterministicJpegTransformationEngine::createValidatedOutput(
    std::vector<std::byte> source, JpegImageAnalysis approvedAnalysis, std::stop_token cancellation) const
{
    using Result = ImageProcessingResult<ValidatedJpegOutput>;
    ++invocations_;
    const auto cancelled = [](const CoefficientTransformationExecutionState executionState) {
        return Result::failure(
            {ImageProcessingErrorCode::Cancelled, ImageProcessingStage::CoefficientTransformation, {}, executionState});
    };
    if (cancellation.stop_requested())
        return cancelled(CoefficientTransformationExecutionState::NotStarted);
    const auto active = ++activeCalls_;
    auto maximum = maximumConcurrentCalls_.load();
    while (maximum < active && !maximumConcurrentCalls_.compare_exchange_weak(maximum, active))
    {
    }
    // Release concurrency observation even when a test factory throws; do not
    // disguise fixture defects as a production domain failure.
    struct ActiveCall final
    {
        std::atomic<unsigned int> &count;
        ~ActiveCall()
        {
            --count;
        }
    } activeCall{activeCalls_};
    auto result = outputFactory_(std::move(source), std::move(approvedAnalysis), cancellation);
    // Keep the real factory's error/admission evidence. Only a successful
    // complete transformation can establish this adapter's late-cancel boundary.
    if (result.valueIfPresent() && cancellation.stop_requested())
        return cancelled(CoefficientTransformationExecutionState::Started);
    return result;
}
} // namespace jpg_spinner::test_support
