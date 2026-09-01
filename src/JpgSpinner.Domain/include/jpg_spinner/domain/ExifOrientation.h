#pragma once

#include "LosslessTransform.h"

#include <cstdint>
#include <optional>

namespace jpg_spinner::domain
{
/// Values are the Exif/TIFF Orientation tag encodings. Each name states
/// where the stored image's 0th row and 0th column appear in the intended
/// display, in that order. For example, RightTop means the 0th row belongs
/// at display-right and the 0th column belongs at display-top.
enum class ExifOrientation : std::uint8_t
{
    TopLeft = 1,
    TopRight = 2,
    BottomRight = 3,
    BottomLeft = 4,
    LeftTop = 5,
    RightTop = 6,
    RightBottom = 7,
    LeftBottom = 8,
};

/// Converts a raw Exif Orientation value only when it is one of the eight
/// values defined by the Exif standard. Invalid or reserved values remain
/// explicit absence; they are never treated as already upright.
[[nodiscard]] std::optional<ExifOrientation> tryParseExifOrientation(std::uint16_t rawOrientation) noexcept;

/// Returns the DCT-coefficient transform that changes the stored
/// orientation into TopLeft while preserving the intended displayed image.
[[nodiscard]] LosslessTransform losslessTransformFor(ExifOrientation sourceOrientation) noexcept;
} // namespace jpg_spinner::domain
