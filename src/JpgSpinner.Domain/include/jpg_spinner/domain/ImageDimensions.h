#pragma once

#include <cstdint>

namespace jpg_spinner::domain
{
/// Strong horizontal pixel extent. Keeping width and height as different
/// types prevents accidental axis interchange in orientation arithmetic.
struct PixelWidth final
{
    std::uint64_t pixels;

    [[nodiscard]] friend constexpr bool operator==(const PixelWidth &, const PixelWidth &) noexcept = default;
};

/// Strong vertical pixel extent. The unsigned representation matches JPEG
/// dimensions after the scanner has rejected absent or zero dimensions.
struct PixelHeight final
{
    std::uint64_t pixels;

    [[nodiscard]] friend constexpr bool operator==(const PixelHeight &, const PixelHeight &) noexcept = default;
};

struct ImageDimensions final
{
    PixelWidth width;
    PixelHeight height;

    [[nodiscard]] friend constexpr bool operator==(const ImageDimensions &, const ImageDimensions &) noexcept = default;
};

/// Sampling factors determine the iMCU geometry. It is supplied explicitly
/// so transform policy never guesses 8x8, 16x8, or 16x16 from an unrelated
/// image property.
struct InterleavedMinimumCodedUnitDimensions final
{
    PixelWidth width;
    PixelHeight height;

    [[nodiscard]] friend constexpr bool operator==(const InterleavedMinimumCodedUnitDimensions &,
                                                   const InterleavedMinimumCodedUnitDimensions &) noexcept = default;
};

/// Exact source-edge extents that a trimming transform will discard.
/// These are source-space edges even when the transform swaps output axes.
struct DiscardedSourceEdgePixels final
{
    PixelWidth rightEdgeWidth;
    PixelHeight bottomEdgeHeight;

    [[nodiscard]] friend constexpr bool operator==(const DiscardedSourceEdgePixels &,
                                                   const DiscardedSourceEdgePixels &) noexcept = default;
};
} // namespace jpg_spinner::domain
