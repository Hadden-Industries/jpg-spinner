#pragma once

#include "JpegMarkerInventory.h"
#include "TurboJpegResourceLimits.h"
#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/JpegTransformPlan.h>
#include <jpg_spinner/domain/ValidatedJpegOutput.h>
#include <stop_token>

namespace jpg_spinner::jpeg::internal
{
/// Approves a completed immutable codestream with a fresh scanner, native
/// metadata reader and full native decode. Source inventory must describe these
/// exact source bytes. Both borrows remain synchronous; only output ownership
/// escapes. Corruption yields OutputValidationFailed with a symbolic rule;
/// cancellation yields Cancelled and never a partial approval capability.
class JpegOutputValidator final
{
  public:
    JpegOutputValidator() = delete;
    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<jpg_spinner::domain::ValidatedJpegOutput> validate(
        std::span<const std::byte> source, const JpegMarkerInventory &sourceInventory,
        const jpg_spinner::domain::JpegTransformPlan &plan, std::vector<std::byte> output,
        std::stop_token cancellation = {},
        const jpg_spinner::domain::JpegResourceLimits &limits = jpg_spinner::domain::JpegResourceLimits::production(),
        const TurboJpegResourceLimits &codecLimits = TurboJpegResourceLimits::production());
};
} // namespace jpg_spinner::jpeg::internal
