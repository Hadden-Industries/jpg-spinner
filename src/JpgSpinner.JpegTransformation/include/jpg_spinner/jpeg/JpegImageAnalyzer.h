#pragma once

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/JpegImageAnalysis.h>
#include <jpg_spinner/domain/JpegResourceLimits.h>
#include <cstddef>
#include <span>

namespace jpg_spinner::jpeg
{
/// Bounded, read-only preflight of one immutable encoded source. Success means
/// supported by current source/metadata/edge policies, not validated output or
/// permission to overwrite a file. Unsupported input returns a typed error.
class JpegImageAnalyzer final
{
  public:
    JpegImageAnalyzer() = delete;

    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<jpg_spinner::domain::JpegImageAnalysis> analyze(
        std::span<const std::byte> source,
        jpg_spinner::domain::EdgeHandlingPolicy edgeHandlingPolicy =
            jpg_spinner::domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform,
        jpg_spinner::domain::OutputScanOrganization outputScanOrganization =
            jpg_spinner::domain::OutputScanOrganization::PreserveSource,
        const jpg_spinner::domain::JpegResourceLimits &resourceLimits =
            jpg_spinner::domain::JpegResourceLimits::production());
};
} // namespace jpg_spinner::jpeg
