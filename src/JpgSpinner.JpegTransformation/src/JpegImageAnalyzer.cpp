#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include <jpg_spinner/domain/JpegTransformPlanner.h>
#include "internal/JpegSegmentScanner.h"
#include "internal/MetadataReconciler.h"
#include <algorithm>

namespace jpg_spinner::jpeg
{
using namespace jpg_spinner::domain;
ImageProcessingResult<JpegImageAnalysis> JpegImageAnalyzer::analyze(std::span<const std::byte> source,
                                                                    EdgeHandlingPolicy edgeHandlingPolicy,
                                                                    OutputScanOrganization outputScanOrganization,
                                                                    const JpegResourceLimits &resourceLimits)
{
    using Result = ImageProcessingResult<JpegImageAnalysis>;
    const auto scan = internal::JpegSegmentScanner::scan(source, resourceLimits);
    if (const auto *error = scan.errorIfPresent())
        return Result::failure(*error);
    const auto &inventory = *scan.valueIfPresent();
    const auto assetBindings = internal::MetadataReconciler::validateSourceAssetBindings(source, inventory);
    if (const auto *error = assetBindings.errorIfPresent())
        return Result::failure(*error);
    const auto orientation = internal::MetadataReconciler::analyzeOrientation(source, inventory);
    if (const auto *error = orientation.errorIfPresent())
        return Result::failure(*error);
    const auto &frame = inventory.frameHeader();
    std::uint8_t maximumHorizontalSampling = 1, maximumVerticalSampling = 1;
    for (const auto &component : frame.components)
    {
        // libjpeg-turbo 3.2 transupp.c::jtransform_request_workspace normalizes
        // single-component transform geometry to one DCT block, even for
        // grayscale sources carrying non-unit relative sampling factors.
        if (frame.components.size() > 1)
        {
            maximumHorizontalSampling = std::max(maximumHorizontalSampling, component.horizontalSamplingFactor);
            maximumVerticalSampling = std::max(maximumVerticalSampling, component.verticalSamplingFactor);
        }
    }
    const auto plan =
        JpegTransformPlanner::createPlan({orientation.valueIfPresent()->authoritativeOrientation,
                                          frame.dimensions,
                                          {{8ULL * maximumHorizontalSampling}, {8ULL * maximumVerticalSampling}},
                                          edgeHandlingPolicy,
                                          outputScanOrganization},
                                         resourceLimits);
    if (const auto *error = plan.errorIfPresent())
        return Result::failure(*error);
    // Preflight the same reconciler the output operation will use. Native parse
    // success alone cannot prove preservation or authorize an irreversible write.
    const auto metadata = internal::MetadataReconciler::reconcileMarkerSegments(source, inventory,
                                                                                *plan.valueIfPresent(), resourceLimits);
    if (const auto *error = metadata.errorIfPresent())
        return Result::failure(*error);
    std::vector<JpegAnalysisFinding> findings;
    if (metadata.valueIfPresent()->removedEmbeddedThumbnail)
        findings.emplace_back(EmbeddedThumbnailRemovalRequiredFinding{});
    if (orientation.valueIfPresent()->hasOrientationConflict)
        findings.emplace_back(ExifXmpOrientationConflictFinding{
            *orientation.valueIfPresent()->exifOrientation, *orientation.valueIfPresent()->xmpOrientation,
            orientation.valueIfPresent()->authoritativeOrientation});
    const auto &discarded = plan.valueIfPresent()->discardedSourceEdgePixels;
    if (discarded.rightEdgeWidth.pixels != 0 || discarded.bottomEdgeHeight.pixels != 0)
        findings.emplace_back(PartialMinimumCodedUnitTrimRequiredFinding{discarded});
    return Result::success({{frame.codingProcess, frame.samplePrecisionBits, frame.dimensions, frame.components},
                            orientation.valueIfPresent()->authoritativeOrientation,
                            *plan.valueIfPresent(),
                            std::move(findings)});
}
} // namespace jpg_spinner::jpeg
