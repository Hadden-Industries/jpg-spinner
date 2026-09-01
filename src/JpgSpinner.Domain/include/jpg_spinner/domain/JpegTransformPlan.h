#pragma once

#include "EdgeHandlingPolicy.h"
#include "ImageDimensions.h"
#include "LosslessTransform.h"
#include "OutputScanOrganization.h"

namespace jpg_spinner::domain
{
/// Fully resolved coefficient-transform policy. Adapters consume this plan
/// without re-deriving orientation or deciding whether data loss is allowed.
struct JpegTransformPlan final
{
    const LosslessTransform transform;
    const ImageDimensions sourceDimensions;
    const ImageDimensions outputDimensions;
    const EdgeHandlingPolicy edgeHandlingPolicy;
    const OutputScanOrganization outputScanOrganization;
    const InterleavedMinimumCodedUnitDimensions sourceInterleavedMinimumCodedUnitDimensions;
    const DiscardedSourceEdgePixels discardedSourceEdgePixels;

    [[nodiscard]] constexpr bool willDiscardEdgePixels() const noexcept
    {
        return discardedSourceEdgePixels.rightEdgeWidth.pixels != 0 ||
               discardedSourceEdgePixels.bottomEdgeHeight.pixels != 0;
    }

    [[nodiscard]] constexpr bool isPerfectTransformAvailable() const noexcept
    {
        return !willDiscardEdgePixels();
    }
};
} // namespace jpg_spinner::domain
