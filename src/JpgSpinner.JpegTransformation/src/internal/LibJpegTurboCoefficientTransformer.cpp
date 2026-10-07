#include "LibJpegTurboCoefficientTransformer.h"
#include "JpegSegmentScanner.h"
#include "TurboJpegResourceLimits.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <turbojpeg.h>

namespace jpg_spinner::jpeg::internal
{
using namespace jpg_spinner::domain;
namespace
{
using TransformResult = ImageProcessingResult<std::size_t>;

[[nodiscard]] TransformResult fail(
    ImageProcessingErrorCode code,
    CoefficientTransformationExecutionState executionState = CoefficientTransformationExecutionState::NotStarted)
{
    if (code == ImageProcessingErrorCode::Cancelled)
        return TransformResult::failure({code, ImageProcessingStage::CoefficientTransformation, {}, executionState});
    return TransformResult::failure({code, ImageProcessingStage::CoefficientTransformation});
}
} // namespace

ImageProcessingResult<std::size_t> LibJpegTurboCoefficientTransformer::transformCoefficients(
    std::span<const std::byte> source, const JpegTransformPlan &plan, std::span<std::byte> destination,
    std::stop_token cancellation, const JpegResourceLimits &resourceLimits,
    const TurboJpegResourceLimits &codecResourceLimits)
{
    if (cancellation.stop_requested())
        return fail(ImageProcessingErrorCode::Cancelled);
    // Zero means unlimited in TurboJPEG, which is not this adapter's policy.
    if (codecResourceLimits.maximumIntermediateBufferMemoryMebibytes <= 0)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    // Enforce the encoded bound before either structural or native parsing.
    if (source.size() > resourceLimits.maximumEncodedFileLengthBytes)
        return fail(ImageProcessingErrorCode::EncodedFileTooLarge);
    const auto inventory = JpegSegmentScanner::scan(source, resourceLimits);
    if (const auto *error = inventory.errorIfPresent())
        return TransformResult::failure(*error);
    const auto &frame = inventory.valueIfPresent()->frameHeader();
    if (frame.dimensions != plan.sourceDimensions)
        return fail(ImageProcessingErrorCode::SourceChangedAfterAnalysis);
    if (plan.edgeHandlingPolicy == EdgeHandlingPolicy::RequirePerfectCoefficientTransform &&
        !plan.isPerfectTransformAvailable())
        return fail(ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable);

    const std::unique_ptr<void, decltype(&tj3Destroy)> context(tj3Init(TJINIT_TRANSFORM), &tj3Destroy);
    if (!context)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    const auto maximumPixels =
        static_cast<int>(std::min<std::uint64_t>(resourceLimits.maximumPixelCount, std::numeric_limits<int>::max()));
    const auto maximumScans = static_cast<int>(
        std::min<std::uint32_t>(resourceLimits.maximumProgressiveScanCount, std::numeric_limits<int>::max()));
    if (tj3Set(context.get(), TJPARAM_STOPONWARNING, 1) != 0 ||
        tj3Set(context.get(), TJPARAM_MAXMEMORY, codecResourceLimits.maximumIntermediateBufferMemoryMebibytes) != 0 ||
        tj3Set(context.get(), TJPARAM_MAXPIXELS, maximumPixels) != 0 ||
        tj3Set(context.get(), TJPARAM_SCANLIMIT, maximumScans) != 0 ||
        tj3Set(context.get(), TJPARAM_SAVEMARKERS, 0) != 0 || tj3Set(context.get(), TJPARAM_NOREALLOC, 1) != 0)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    const auto *encodedSource = reinterpret_cast<const unsigned char *>(source.data());
    if (tj3DecompressHeader(context.get(), encodedSource, source.size()) != 0)
        return fail(ImageProcessingErrorCode::MalformedJpegStructure);

    // T.81 B.2.4.4 defines DRI's two-byte MCU interval. TurboJPEG exposes
    // output restart configuration but not the source interval through Get.
    // Use only scanner-bounded DRI payloads; do not parse entropy or markers
    // again. A fixed interval preserves the MCU count after axis exchange.
    unsigned int restartInterval = 0;
    bool hasSeenScan = false;
    for (const auto &marker : inventory.valueIfPresent()->markers())
    {
        if (marker.markerCode == 0xda)
            hasSeenScan = true;
        if (marker.markerCode != 0xdd)
            continue;
        if (marker.payloadRange.lengthBytes != 2)
            return fail(ImageProcessingErrorCode::MalformedJpegStructure);
        const auto offset = static_cast<std::size_t>(marker.payloadRange.offsetBytes);
        const auto interval =
            (std::to_integer<unsigned int>(source[offset]) << 8) | std::to_integer<unsigned int>(source[offset + 1]);
        // One output parameter cannot preserve scan-dependent changes. Fail
        // explicitly rather than silently removing their recovery boundaries.
        if (hasSeenScan && interval != restartInterval)
            return fail(ImageProcessingErrorCode::UnsupportedJpegRestartIntervalChanges);
        restartInterval = interval;
    }
    if (tj3Set(context.get(), TJPARAM_RESTARTBLOCKS, static_cast<int>(restartInterval)) != 0)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);

    tjtransform transform{};
    // The domain enum follows Exif orientation order, not TurboJPEG's order.
    // Keep an explicit mapping rather than coupling their integer encodings.
    switch (plan.transform)
    {
    case LosslessTransform::None:
        transform.op = TJXOP_NONE;
        break;
    case LosslessTransform::FlipHorizontal:
        transform.op = TJXOP_HFLIP;
        break;
    case LosslessTransform::Rotate180:
        transform.op = TJXOP_ROT180;
        break;
    case LosslessTransform::FlipVertical:
        transform.op = TJXOP_VFLIP;
        break;
    case LosslessTransform::Transpose:
        transform.op = TJXOP_TRANSPOSE;
        break;
    case LosslessTransform::Rotate90Clockwise:
        transform.op = TJXOP_ROT90;
        break;
    case LosslessTransform::Transverse:
        transform.op = TJXOP_TRANSVERSE;
        break;
    case LosslessTransform::Rotate270Clockwise:
        transform.op = TJXOP_ROT270;
        break;
    default:
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    }
    transform.options =
        TJXOPT_COPYNONE |
        (plan.edgeHandlingPolicy == EdgeHandlingPolicy::RequirePerfectCoefficientTransform ? TJXOPT_PERFECT
                                                                                           : TJXOPT_TRIM);
    const bool sourceProgressive = frame.codingProcess == JpegCodingProcess::ProgressiveDctHuffman ||
                                   frame.codingProcess == JpegCodingProcess::ProgressiveDctArithmetic;
    const bool sourceArithmetic = frame.codingProcess == JpegCodingProcess::ExtendedSequentialDctArithmetic ||
                                  frame.codingProcess == JpegCodingProcess::ProgressiveDctArithmetic;
    if (plan.outputScanOrganization == OutputScanOrganization::ProgressiveDct ||
        (plan.outputScanOrganization == OutputScanOrganization::PreserveSource && sourceProgressive))
        transform.options |= TJXOPT_PROGRESSIVE;
    if (sourceArithmetic)
        transform.options |= TJXOPT_ARITHMETIC;
    // Header inspection changes context flags. Clear those inherited flags
    // so the operation-local options alone select the output coding policy.
    if (tj3Set(context.get(), TJPARAM_PROGRESSIVE, 0) != 0 || tj3Set(context.get(), TJPARAM_ARITHMETIC, 0) != 0)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    const auto worstCaseLength = tj3TransformBufSize(context.get(), &transform);
    if (worstCaseLength == 0)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    const auto capacity = std::min(worstCaseLength, destination.size());
    if (capacity == 0)
        return fail(ImageProcessingErrorCode::OutputBufferTooSmall);
    const std::unique_ptr<unsigned char, decltype(&tj3Free)> encodedOutput(
        static_cast<unsigned char *>(tj3Alloc(capacity)), &tj3Free);
    if (!encodedOutput)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    auto *outputPointer = encodedOutput.get();
    auto outputLength = capacity;
    // TurboJPEG's synchronous C call is not interruptible. Check at both
    // boundaries and publish no bytes if cancellation arrives during the call.
    if (cancellation.stop_requested())
        return fail(ImageProcessingErrorCode::Cancelled);
    const auto status =
        tj3Transform(context.get(), encodedSource, source.size(), 1, &outputPointer, &outputLength, &transform);
    if (cancellation.stop_requested())
        return fail(ImageProcessingErrorCode::Cancelled, CoefficientTransformationExecutionState::Started);
    if (status != 0)
        return fail(ImageProcessingErrorCode::CoefficientTransformationFailed);
    if (outputLength > destination.size())
        return fail(ImageProcessingErrorCode::OutputBufferTooSmall);
    const auto outputInventory = JpegSegmentScanner::scan(
        {reinterpret_cast<const std::byte *>(encodedOutput.get()), outputLength}, resourceLimits);
    if (!outputInventory.valueIfPresent() ||
        outputInventory.valueIfPresent()->frameHeader().dimensions != plan.outputDimensions)
        return fail(ImageProcessingErrorCode::OutputValidationFailed);
    // NOREALLOC makes the temporary native allocation a strict bound. Delay
    // writing the caller's sink until success so no partial output escapes.
    std::memcpy(destination.data(), encodedOutput.get(), outputLength);
    return TransformResult::success(outputLength);
}
} // namespace jpg_spinner::jpeg::internal
