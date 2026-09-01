#pragma once

#include "ExifOrientation.h"
#include "JpegAnalysisFinding.h"
#include "JpegTransformPlan.h"

#include <vector>

namespace jpg_spinner::domain
{
/// Immutable review artifact produced before a batch can be accepted. It
/// carries the authoritative orientation, resolved plan, and typed facts
/// that require explicit user review; no localized strings enter Domain.
struct JpegImageAnalysis final
{
    const ExifOrientation authoritativeOrientation;
    const JpegTransformPlan transformPlan;
    const std::vector<JpegAnalysisFinding> findings;
};
} // namespace jpg_spinner::domain
