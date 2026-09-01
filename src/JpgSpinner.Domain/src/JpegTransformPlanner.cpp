#include <jpg_spinner/domain/JpegTransformPlanner.h>

#include <jpg_spinner/domain/ExifOrientation.h>

#include <cstdint>
#include <exception>
#include <limits>
#include <optional>

namespace jpg_spinner::domain
{
namespace
{
struct AffectedSourceEdges final
{
    bool affectsRightEdge;
    bool affectsBottomEdge;
};

[[nodiscard]] AffectedSourceEdges affectedSourceEdgesFor(const LosslessTransform transform) noexcept
{
    // libjpeg-turbo 3.2 defines perfect-transform eligibility per
    // operation: horizontal/270 affect the right edge, vertical/90
    // affect the bottom edge, 180/transverse affect both, and
    // transpose is perfect for any iMCU edge completeness.
    switch (transform)
    {
    case LosslessTransform::None:
    case LosslessTransform::Transpose:
        return {false, false};
    case LosslessTransform::FlipHorizontal:
    case LosslessTransform::Rotate270Clockwise:
        return {true, false};
    case LosslessTransform::FlipVertical:
    case LosslessTransform::Rotate90Clockwise:
        return {false, true};
    case LosslessTransform::Rotate180:
    case LosslessTransform::Transverse:
        return {true, true};
    }

    // LosslessTransform enters through a closed domain enum. Failing
    // closed on a fabricated value is safer than selecting an edge
    // policy that could silently discard or rearrange source pixels.
    std::terminate();
}

[[nodiscard]] constexpr bool swapsAxes(const LosslessTransform transform) noexcept
{
    switch (transform)
    {
    case LosslessTransform::Transpose:
    case LosslessTransform::Rotate90Clockwise:
    case LosslessTransform::Transverse:
    case LosslessTransform::Rotate270Clockwise:
        return true;
    case LosslessTransform::None:
    case LosslessTransform::FlipHorizontal:
    case LosslessTransform::Rotate180:
    case LosslessTransform::FlipVertical:
        return false;
    }

    std::terminate();
}

[[nodiscard]] ImageProcessingResult<JpegTransformPlan> malformedGeometryError() noexcept
{
    return ImageProcessingResult<JpegTransformPlan>::failure(ImageProcessingError{
        ImageProcessingErrorCode::MalformedJpegStructure,
        ImageProcessingStage::TransformPlanning,
    });
}

[[nodiscard]] ImageProcessingResult<JpegTransformPlan> pixelCountLimitError(
    const std::optional<std::uint64_t> observedPixelCount, const std::uint64_t maximumPixelCount) noexcept
{
    return ImageProcessingResult<JpegTransformPlan>::failure(ImageProcessingError{
        ImageProcessingErrorCode::PixelCountLimitExceeded,
        ImageProcessingStage::TransformPlanning,
        {},
        JpegResourceLimitViolation{
            JpegResourceLimit::PixelCount,
            observedPixelCount,
            maximumPixelCount,
        },
    });
}

[[nodiscard]] ImageProcessingResult<JpegTransformPlan> perfectTransformUnavailableError() noexcept
{
    return ImageProcessingResult<JpegTransformPlan>::failure(ImageProcessingError{
        ImageProcessingErrorCode::PerfectCoefficientTransformUnavailable,
        ImageProcessingStage::TransformPlanning,
    });
}
} // namespace

ImageProcessingResult<JpegTransformPlan> JpegTransformPlanner::createPlan(
    const JpegTransformRequest &request, const JpegResourceLimits &resourceLimits) noexcept
{
    const auto sourceWidthPixels = request.sourceDimensions.width.pixels;
    const auto sourceHeightPixels = request.sourceDimensions.height.pixels;
    const auto iMcuWidthPixels = request.sourceInterleavedMinimumCodedUnitDimensions.width.pixels;
    const auto iMcuHeightPixels = request.sourceInterleavedMinimumCodedUnitDimensions.height.pixels;

    // JPEG dimensions and sampling-derived iMCU dimensions must be
    // positive. This guard also proves every later remainder divisor is
    // non-zero before arithmetic touches untrusted scanner output.
    if (sourceWidthPixels == 0 || sourceHeightPixels == 0 || iMcuWidthPixels == 0 || iMcuHeightPixels == 0)
    {
        return malformedGeometryError();
    }

    // Division proves multiplication is representable before it occurs.
    // Overflow is reported without a fictional observed product.
    if (sourceWidthPixels > std::numeric_limits<std::uint64_t>::max() / sourceHeightPixels)
    {
        return pixelCountLimitError(std::nullopt, resourceLimits.maximumPixelCount);
    }

    const auto sourcePixelCount = sourceWidthPixels * sourceHeightPixels;
    if (sourcePixelCount > resourceLimits.maximumPixelCount)
    {
        return pixelCountLimitError(sourcePixelCount, resourceLimits.maximumPixelCount);
    }

    const auto transform = losslessTransformFor(request.sourceOrientation);
    const auto affectedSourceEdges = affectedSourceEdgesFor(transform);
    const auto partialRightEdgeWidthPixels = sourceWidthPixels % iMcuWidthPixels;
    const auto partialBottomEdgeHeightPixels = sourceHeightPixels % iMcuHeightPixels;
    const auto requiredRightEdgeDiscardPixels = affectedSourceEdges.affectsRightEdge ? partialRightEdgeWidthPixels : 0;
    const auto requiredBottomEdgeDiscardPixels =
        affectedSourceEdges.affectsBottomEdge ? partialBottomEdgeHeightPixels : 0;
    const auto wouldDiscardEdgePixels = requiredRightEdgeDiscardPixels != 0 || requiredBottomEdgeDiscardPixels != 0;

    if (wouldDiscardEdgePixels && request.edgeHandlingPolicy == EdgeHandlingPolicy::RequirePerfectCoefficientTransform)
    {
        // This is the domain equivalent of TurboJPEG's TJXOPT_PERFECT:
        // reject an operation whose affected edge contains a partial iMCU
        // rather than allowing the codec to leave an untransformed strip.
        return perfectTransformUnavailableError();
    }

    if (requiredRightEdgeDiscardPixels == sourceWidthPixels || requiredBottomEdgeDiscardPixels == sourceHeightPixels)
    {
        // libjpeg-turbo 3.2's trim_right_edge() and trim_bottom_edge()
        // intentionally retain the edge when no complete iMCU exists. Reject
        // that case because a zero-sized plan cannot be executed, while
        // accepting the codec's retained strip would leave the requested
        // orientation transform incomplete.
        return perfectTransformUnavailableError();
    }

    const DiscardedSourceEdgePixels discardedSourceEdgePixels{
        PixelWidth{requiredRightEdgeDiscardPixels},
        PixelHeight{requiredBottomEdgeDiscardPixels},
    };
    const ImageDimensions retainedSourceDimensions{
        PixelWidth{sourceWidthPixels - requiredRightEdgeDiscardPixels},
        PixelHeight{sourceHeightPixels - requiredBottomEdgeDiscardPixels},
    };
    const auto outputDimensions = swapsAxes(transform)
                                          ? ImageDimensions{
                                                PixelWidth{retainedSourceDimensions.height.pixels},
                                                PixelHeight{retainedSourceDimensions.width.pixels},
                                            }
                                          : retainedSourceDimensions;

    return ImageProcessingResult<JpegTransformPlan>::success(JpegTransformPlan{
        transform,
        request.sourceDimensions,
        outputDimensions,
        request.edgeHandlingPolicy,
        request.outputScanOrganization,
        request.sourceInterleavedMinimumCodedUnitDimensions,
        discardedSourceEdgePixels,
    });
}
} // namespace jpg_spinner::domain
