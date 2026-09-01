#pragma once

#include "ImageProcessingError.h"

#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>

namespace jpg_spinner::domain
{
/// Project-owned result with exactly one active value or structured error.
/// Its deliberately small interface is a domain contract, not a shim for
/// std::expected or a third-party monadic result library.
template <typename TValue> class ImageProcessingResult final
{
  public:
    ImageProcessingResult() = delete;
    ImageProcessingResult(const ImageProcessingResult &) = default;
    ImageProcessingResult(ImageProcessingResult &&) = default;

    // Assignment is intentionally absent. Construct-once result objects
    // preserve the never-empty invariant without exposing variant state
    // transitions that can become valueless when an alternative throws.
    ImageProcessingResult &operator=(const ImageProcessingResult &) = delete;
    ImageProcessingResult &operator=(ImageProcessingResult &&) = delete;

    [[nodiscard]] static ImageProcessingResult success(TValue value) noexcept(
        std::is_nothrow_move_constructible_v<TValue>)
    {
        return ImageProcessingResult(std::in_place_index<0>, std::move(value));
    }

    [[nodiscard]] static ImageProcessingResult failure(ImageProcessingError error) noexcept
    {
        return ImageProcessingResult(std::in_place_index<1>, std::move(error));
    }

    [[nodiscard]] TValue *valueIfPresent() noexcept
    {
        return std::get_if<0>(&valueOrError_);
    }

    [[nodiscard]] const TValue *valueIfPresent() const noexcept
    {
        return std::get_if<0>(&valueOrError_);
    }

    [[nodiscard]] ImageProcessingError *errorIfPresent() noexcept
    {
        return std::get_if<1>(&valueOrError_);
    }

    [[nodiscard]] const ImageProcessingError *errorIfPresent() const noexcept
    {
        return std::get_if<1>(&valueOrError_);
    }

  private:
    template <std::size_t AlternativeIndex, typename TAlternative>
    explicit ImageProcessingResult(std::in_place_index_t<AlternativeIndex>, TAlternative &&alternative) noexcept(
        std::is_nothrow_constructible_v<std::variant<TValue, ImageProcessingError>,
                                        std::in_place_index_t<AlternativeIndex>, TAlternative &&>)
        : valueOrError_(std::in_place_index<AlternativeIndex>, std::forward<TAlternative>(alternative))
    {
    }

    // Move construction of std::variant leaves the source holding the same
    // alternative (whose value may itself be moved from), so both source
    // and destination remain inspectable result objects.
    std::variant<TValue, ImageProcessingError> valueOrError_;
};
} // namespace jpg_spinner::domain
