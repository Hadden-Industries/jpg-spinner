#pragma once

#include "ImageProcessingResult.h"
#include "JpegResourceLimits.h"
#include "JpegTransformPlan.h"
#include "JpegTransformRequest.h"

namespace jpg_spinner::domain
{
/// Pure policy module that resolves Exif orientation, iMCU edge rules,
/// checked dimension arithmetic, and output-axis geometry in one operation.
class JpegTransformPlanner final
{
  public:
    JpegTransformPlanner() = delete;

    [[nodiscard]] static ImageProcessingResult<JpegTransformPlan> createPlan(
        const JpegTransformRequest &request,
        const JpegResourceLimits &resourceLimits = JpegResourceLimits::production()) noexcept;
};
} // namespace jpg_spinner::domain
