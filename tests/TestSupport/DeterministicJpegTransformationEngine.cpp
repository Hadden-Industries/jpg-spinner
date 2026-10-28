#include "DeterministicJpegTransformationEngine.h"
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <stdexcept>

namespace jpg_spinner::test_support
{
std::uint64_t DeterministicJpegTransformationEngine::estimatedWorkingMemoryBytes(
    const std::uint64_t encodedSourceLengthBytes, const jpg_spinner::domain::JpegImageAnalysis &analysis) const noexcept
{
    return jpg_spinner::jpeg::LibJpegTurboTransformationEngine{}.estimatedWorkingMemoryBytes(encodedSourceLengthBytes,
                                                                                             analysis);
}
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
