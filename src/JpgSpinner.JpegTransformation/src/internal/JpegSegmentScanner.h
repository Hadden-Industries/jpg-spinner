#pragma once

#include "JpegMarkerInventory.h"

#include <jpg_spinner/domain/ImageProcessingResult.h>
#include <jpg_spinner/domain/JpegResourceLimits.h>

#include <cstddef>
#include <span>

namespace jpg_spinner::jpeg::internal
{
/// Performs one bounded structural pass over caller-owned encoded bytes. It
/// never calls a codec or metadata parser and never retains borrowed storage.
/// Success inventories only the first codestream; callers must check trailing
/// data and metadata semantics before authorizing any transform.
class JpegSegmentScanner final
{
  public:
    JpegSegmentScanner() = delete;

    [[nodiscard]] static jpg_spinner::domain::ImageProcessingResult<JpegMarkerInventory> scan(
        std::span<const std::byte> encodedJpeg, const jpg_spinner::domain::JpegResourceLimits &resourceLimits =
                                                    jpg_spinner::domain::JpegResourceLimits::production());
};
} // namespace jpg_spinner::jpeg::internal
