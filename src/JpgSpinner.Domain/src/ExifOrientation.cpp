#include <jpg_spinner/domain/ExifOrientation.h>

#include <exception>

namespace jpg_spinner::domain
{
std::optional<ExifOrientation> tryParseExifOrientation(const std::uint16_t rawOrientation) noexcept
{
    switch (rawOrientation)
    {
    case 1:
        return ExifOrientation::TopLeft;
    case 2:
        return ExifOrientation::TopRight;
    case 3:
        return ExifOrientation::BottomRight;
    case 4:
        return ExifOrientation::BottomLeft;
    case 5:
        return ExifOrientation::LeftTop;
    case 6:
        return ExifOrientation::RightTop;
    case 7:
        return ExifOrientation::RightBottom;
    case 8:
        return ExifOrientation::LeftBottom;
    default:
        return std::nullopt;
    }
}

LosslessTransform losslessTransformFor(const ExifOrientation sourceOrientation) noexcept
{
    switch (sourceOrientation)
    {
    case ExifOrientation::TopLeft:
        return LosslessTransform::None;
    case ExifOrientation::TopRight:
        return LosslessTransform::FlipHorizontal;
    case ExifOrientation::BottomRight:
        return LosslessTransform::Rotate180;
    case ExifOrientation::BottomLeft:
        return LosslessTransform::FlipVertical;
    case ExifOrientation::LeftTop:
        // Orientation 5 reflects across the upper-left/lower-right axis.
        // A rotation alone would reverse one of the encoded row/column
        // directions, so this is specifically a transpose.
        return LosslessTransform::Transpose;
    case ExifOrientation::RightTop:
        return LosslessTransform::Rotate90Clockwise;
    case ExifOrientation::RightBottom:
        // Orientation 7 reflects across the upper-right/lower-left axis.
        // It is the anti-diagonal counterpart of Transpose, not a rotation.
        return LosslessTransform::Transverse;
    case ExifOrientation::LeftBottom:
        return LosslessTransform::Rotate270Clockwise;
    }

    // All values can enter this function only through the strongly typed
    // ExifOrientation contract. Terminating on an invalid cast prevents a
    // fabricated enum value from being silently normalized as TopLeft.
    std::terminate();
}
} // namespace jpg_spinner::domain
