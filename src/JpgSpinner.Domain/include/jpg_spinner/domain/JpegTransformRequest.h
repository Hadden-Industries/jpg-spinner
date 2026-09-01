#pragma once

#include "EdgeHandlingPolicy.h"
#include "ExifOrientation.h"
#include "ImageDimensions.h"
#include "OutputScanOrganization.h"

namespace jpg_spinner::domain
{
/// Complete immutable input to pure transform planning. Codec adapters
/// provide the sampling-derived iMCU dimensions; the planner performs no
/// parsing and has no hidden environment-dependent defaults.
struct JpegTransformRequest final
{
    const ExifOrientation sourceOrientation;
    const ImageDimensions sourceDimensions;
    const InterleavedMinimumCodedUnitDimensions sourceInterleavedMinimumCodedUnitDimensions;
    const EdgeHandlingPolicy edgeHandlingPolicy;
    const OutputScanOrganization outputScanOrganization;
};
} // namespace jpg_spinner::domain
