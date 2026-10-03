#pragma once

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/JpegImageAnalysis.h>
#include <jpg_spinner/domain/ValidatedJpegOutput.h>
#include <stop_token>

namespace jpg_spinner::jpeg
{
/// Application boundary for one approved JPEG transformation. Both inputs are
/// owned values, suitable for transfer into later coroutine/batch work. Success
/// owns fully validated bytes; failure/cancellation publishes no intermediate
/// output. Implementations have no filesystem or transaction authority.
class JpegTransformationEngine
{
  public:
    virtual ~JpegTransformationEngine() = default;
    [[nodiscard]] virtual jpg_spinner::domain::ImageProcessingResult<jpg_spinner::domain::ValidatedJpegOutput>
    createValidatedOutput(std::vector<std::byte> source, jpg_spinner::domain::JpegImageAnalysis approvedAnalysis,
                          std::stop_token cancellation = {}) const = 0;
};
} // namespace jpg_spinner::jpeg
