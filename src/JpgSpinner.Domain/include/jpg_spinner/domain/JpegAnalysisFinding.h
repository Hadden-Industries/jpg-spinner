#pragma once

#include "ExifOrientation.h"
#include "ImageDimensions.h"

#include <exception>
#include <variant>

namespace jpg_spinner::domain
{
enum class JpegAnalysisFindingCode
{
    ExifXmpOrientationConflict,
    EmbeddedThumbnailRemovalRequired,
    PartialMinimumCodedUnitTrimRequired,
};

struct ExifXmpOrientationConflictFinding final
{
    const ExifOrientation exifOrientation;
    const ExifOrientation xmpOrientation;
    const ExifOrientation authoritativeOrientation;
};

/// Marker finding: any embedded thumbnail must be removed because its
/// pixels and orientation metadata would otherwise disagree with output.
struct EmbeddedThumbnailRemovalRequiredFinding final
{
};

struct PartialMinimumCodedUnitTrimRequiredFinding final
{
    const DiscardedSourceEdgePixels discardedSourceEdgePixels;
};

using JpegAnalysisFinding = std::variant<ExifXmpOrientationConflictFinding, EmbeddedThumbnailRemovalRequiredFinding,
                                         PartialMinimumCodedUnitTrimRequiredFinding>;

[[nodiscard]] constexpr JpegAnalysisFindingCode jpegAnalysisFindingCode(const JpegAnalysisFinding &finding) noexcept
{
    switch (finding.index())
    {
    case 0:
        return JpegAnalysisFindingCode::ExifXmpOrientationConflict;
    case 1:
        return JpegAnalysisFindingCode::EmbeddedThumbnailRemovalRequired;
    case 2:
        return JpegAnalysisFindingCode::PartialMinimumCodedUnitTrimRequired;
    default:
        // std::variant can be valueless only after a throwing assignment;
        // this immutable finding representation exposes no assignment.
        std::terminate();
    }
}
} // namespace jpg_spinner::domain
