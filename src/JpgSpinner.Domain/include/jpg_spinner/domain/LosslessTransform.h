#pragma once

namespace jpg_spinner::domain
{
/// Describes a spatial transform performed directly on JPEG DCT
/// coefficients, without decoding and recompressing image samples.
enum class LosslessTransform
{
    None,
    FlipHorizontal,
    Rotate180,
    FlipVertical,
    Transpose,
    Rotate90Clockwise,
    Transverse,
    Rotate270Clockwise,
};
} // namespace jpg_spinner::domain
