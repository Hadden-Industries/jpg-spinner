#pragma once

#include "TurboJpegResourceLimits.h"

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/JpegTransformPlan.h>
#include <cstddef>
#include <span>
#include <stop_token>

namespace jpg_spinner::jpeg::internal
{
/// Internal codec adapter; metadata reconciliation is a separate operation.
/// Source and destination remain caller-owned and must not change concurrently.
/// Success returns the written prefix length; failure leaves destination intact.
/// The caller must authorize metadata/trailing-payload policy before invoking
/// this seam. It cannot access files or retain a codec-owned buffer.
class LibJpegTurboCoefficientTransformer final
{
  public:
    LibJpegTurboCoefficientTransformer() = delete;

    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<std::size_t> transformCoefficients(
        std::span<const std::byte> source, const jpg_spinner::domain::JpegTransformPlan &plan,
        std::span<std::byte> destination, std::stop_token cancellation = {},
        const jpg_spinner::domain::JpegResourceLimits &resourceLimits =
            jpg_spinner::domain::JpegResourceLimits::production(),
        const TurboJpegResourceLimits &codecResourceLimits = TurboJpegResourceLimits::production());
};
} // namespace jpg_spinner::jpeg::internal
