#pragma once

#include "JpegTransformationEngine.h"

namespace jpg_spinner::jpeg
{
/// Thread-safe composition of the bounded native coefficient transform,
/// metadata reconciliation and independent output validator. Per-call codec
/// state is owned locally; Exiv2's process-global registry is serialized inside
/// the metadata module. Approved analysis is rechecked before native work.
class LibJpegTurboTransformationEngine final : public JpegTransformationEngine
{
  public:
    explicit LibJpegTurboTransformationEngine(jpg_spinner::domain::JpegResourceLimits resourceLimits =
                                                  jpg_spinner::domain::JpegResourceLimits::production()) noexcept
        : resourceLimits_(resourceLimits)
    {
    }
    [[nodiscard]] jpg_spinner::domain::ImageProcessingResult<jpg_spinner::domain::ValidatedJpegOutput>
    createValidatedOutput(std::vector<std::byte> source, jpg_spinner::domain::JpegImageAnalysis approvedAnalysis,
                          std::stop_token cancellation = {}) const override;

  private:
    const jpg_spinner::domain::JpegResourceLimits resourceLimits_;
};
} // namespace jpg_spinner::jpeg
