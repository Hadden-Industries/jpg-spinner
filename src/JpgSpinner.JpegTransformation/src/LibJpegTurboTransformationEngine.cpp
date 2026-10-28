#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <jpg_spinner/jpeg/JpegImageAnalyzer.h>
#include "internal/JpegOutputValidator.h"
#include "internal/JpegSegmentScanner.h"
#include "internal/LibJpegTurboCoefficientTransformer.h"
#include "internal/MetadataReconciler.h"
#include "internal/TurboJpegResourceLimits.h"
#include <array>
#include <algorithm>
#include <limits>
#include <new>
#include <turbojpeg.h>

namespace jpg_spinner::jpeg
{
using namespace jpg_spinner::domain;
std::uint64_t LibJpegTurboTransformationEngine::outputBufferCapacityBytes(
    const ImageDimensions &dimensions) const noexcept
{
    if (dimensions.width.pixels > std::numeric_limits<int>::max() ||
        dimensions.height.pixels > std::numeric_limits<int>::max())
        return 0;
    const auto bound = tj3JPEGBufSize(static_cast<int>(dimensions.width.pixels),
                                      static_cast<int>(dimensions.height.pixels), TJSAMP_444);
    return std::min<std::uint64_t>(bound, resourceLimits_.maximumEncodedFileLengthBytes);
}
std::uint64_t LibJpegTurboTransformationEngine::estimatedWorkingMemoryBytes(
    const std::uint64_t encodedSourceLengthBytes, const JpegImageAnalysis &analysis) const noexcept
{
    constexpr auto unknown = std::numeric_limits<std::uint64_t>::max();
    // Reuse the same native output bound and intermediate-memory policy as the
    // actual transform. Allow overlapping source/output buffers and metadata
    // representations conservatively, without claiming to measure total RAM.
    const auto output = outputBufferCapacityBytes(analysis.transformPlan.outputDimensions);
    if (output == 0)
        return unknown;
    const auto metadata = std::min(encodedSourceLengthBytes, resourceLimits_.maximumMetadataLengthBytes);
    const auto intermediate =
        static_cast<std::uint64_t>(
            internal::TurboJpegResourceLimits::production().maximumIntermediateBufferMemoryMebibytes) *
        1024 * 1024;
    std::uint64_t estimate{};
    for (const auto bytes : std::array{encodedSourceLengthBytes, encodedSourceLengthBytes, output, output, intermediate,
                                       metadata, metadata, metadata, metadata})
    {
        if (bytes > unknown - estimate)
            return unknown; // Unknown/oversized estimates admit only exclusive work.
        estimate += bytes;
    }
    return estimate;
}
ImageProcessingResult<ValidatedJpegOutput> LibJpegTurboTransformationEngine::createValidatedOutput(
    std::vector<std::byte> source, JpegImageAnalysis approvedAnalysis, std::stop_token cancellation) const
{
    using Result = ImageProcessingResult<ValidatedJpegOutput>;
    auto stage = ImageProcessingStage::TransformPlanning;
    auto executionState = CoefficientTransformationExecutionState::NotStarted;
    const auto cancelled = [&] {
        return Result::failure({ImageProcessingErrorCode::Cancelled, stage, {}, executionState});
    };
    try
    {
        if (cancellation.stop_requested())
            return cancelled();
        const auto &plan = approvedAnalysis.transformPlan;
        // Review artifacts are values, not permission to trust caller-supplied
        // geometry or omit a newly observed destructive metadata finding.
        const auto currentAnalysis =
            JpegImageAnalyzer::analyze(source, plan.edgeHandlingPolicy, plan.outputScanOrganization, resourceLimits_);
        if (const auto *error = currentAnalysis.errorIfPresent())
            return Result::failure(*error);
        const auto &observed = *currentAnalysis.valueIfPresent();
        if (observed.sourceProperties != approvedAnalysis.sourceProperties ||
            observed.authoritativeOrientation != approvedAnalysis.authoritativeOrientation ||
            observed.transformPlan != plan || observed.findings != approvedAnalysis.findings)
            return Result::failure({ImageProcessingErrorCode::SourceChangedAfterAnalysis, stage});
        const auto scan = internal::JpegSegmentScanner::scan(source, resourceLimits_);
        if (const auto *error = scan.errorIfPresent())
            return Result::failure(*error);
        std::vector<std::byte> completed;
        {
            // Release writer metadata and the worst-case codec output allocation
            // before starting the independent reader. The encoded-size and native
            // coefficient budgets are separate bounds, not a process-wide limit.
            stage = ImageProcessingStage::MetadataReconciliation;
            const auto metadata = internal::MetadataReconciler::reconcileMarkerSegments(source, *scan.valueIfPresent(),
                                                                                        plan, resourceLimits_);
            if (const auto *error = metadata.errorIfPresent())
                return Result::failure(*error);
            if (cancellation.stop_requested())
                return cancelled();
            stage = ImageProcessingStage::CoefficientTransformation;
            // The native upper-bound API owns this sizing rule. 4:4:4 is the
            // conservative bound across allowed sampling arrangements. Limit the
            // allocation independently of the native codec working-memory budget.
            const auto capacity = outputBufferCapacityBytes(plan.outputDimensions);
            if (capacity == 0 || capacity > std::numeric_limits<std::size_t>::max())
                return Result::failure({ImageProcessingErrorCode::EncodedFileTooLarge, stage});
            std::vector<std::byte> codecOutput(static_cast<std::size_t>(capacity));
            const auto transformed = internal::LibJpegTurboCoefficientTransformer::transformCoefficients(
                source, plan, codecOutput, cancellation, resourceLimits_);
            if (const auto *error = transformed.errorIfPresent())
                return Result::failure(*error);
            // Success proves tj3Transform returned. Its own cancellation errors
            // already distinguish the checks on either side of that native call.
            executionState = CoefficientTransformationExecutionState::Started;
            if (cancellation.stop_requested())
                return cancelled();
            codecOutput.resize(*transformed.valueIfPresent());
            const auto codecScan = internal::JpegSegmentScanner::scan(codecOutput, resourceLimits_);
            if (!codecScan.valueIfPresent())
                return Result::failure({ImageProcessingErrorCode::OutputValidationFailed,
                                        ImageProcessingStage::OutputValidation,
                                        {},
                                        JpegOutputValidationRule::CompleteJpegStructure});
            const auto &segments = metadata.valueIfPresent()->encodedBytes;
            std::size_t discardedCodecMetadataBytes = 0;
            for (const auto &marker : codecScan.valueIfPresent()->markers())
                if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
                    discardedCodecMetadataBytes += static_cast<std::size_t>(marker.encodedRange.lengthBytes);
            const auto codestreamLength = codecOutput.size() - discardedCodecMetadataBytes;
            if (segments.size() > resourceLimits_.maximumEncodedFileLengthBytes ||
                codestreamLength > resourceLimits_.maximumEncodedFileLengthBytes - segments.size() ||
                codestreamLength > std::numeric_limits<std::size_t>::max() - segments.size())
                return Result::failure({ImageProcessingErrorCode::EncodedFileTooLarge, stage});
            completed.reserve(codestreamLength + segments.size());
            completed.insert(completed.end(), codecOutput.begin(), codecOutput.begin() + 2);
            completed.insert(completed.end(), segments.begin(), segments.end());
            std::size_t cursor = 2;
            // COPYNONE prevents copies but native encoders may generate color-hint
            // APP0/APP14 markers. The reconciler exclusively owns APPn/COM output:
            // replace all generated hints with the reviewed source-relative sequence.
            for (const auto &marker : codecScan.valueIfPresent()->markers())
                if ((marker.markerCode >= 0xe0 && marker.markerCode <= 0xef) || marker.markerCode == 0xfe)
                {
                    const auto offset = static_cast<std::size_t>(marker.encodedRange.offsetBytes);
                    completed.insert(completed.end(), codecOutput.begin() + static_cast<std::ptrdiff_t>(cursor),
                                     codecOutput.begin() + static_cast<std::ptrdiff_t>(offset));
                    cursor = offset + static_cast<std::size_t>(marker.encodedRange.lengthBytes);
                }
            completed.insert(completed.end(), codecOutput.begin() + static_cast<std::ptrdiff_t>(cursor),
                             codecOutput.end());
        }
        stage = ImageProcessingStage::OutputValidation;
        auto validated = internal::JpegOutputValidator::validate(source, *scan.valueIfPresent(), plan,
                                                                 std::move(completed), cancellation, resourceLimits_);
        if (const auto *error = validated.errorIfPresent(); error && error->code == ImageProcessingErrorCode::Cancelled)
            return Result::failure({error->code, error->stage, error->nativeErrorProjection, executionState});
        return validated;
    }
    catch (const std::bad_alloc &)
    {
        return Result::failure({ImageProcessingErrorCode::WorkingMemoryAllocationFailed, stage});
    }
}
} // namespace jpg_spinner::jpeg
