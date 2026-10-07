#pragma once

#include <jpg_spinner/domain/ImageProcessingError.h>
#include <winrt/Microsoft.Windows.ApplicationModel.Resources.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <array>

namespace jpg_spinner::presentation
{
/// Localized text only. Native error messages, diagnostic context and paths are
/// intentionally absent; none can be interpolated into these resources.
struct ImageProcessingErrorText final
{
    winrt::hstring title;
    winrt::hstring explanation;
    winrt::hstring remedy;
    /// Native candidate provenance allows qualification to detect locale fallback
    /// separately for every component, not just the title.
    std::array<winrt::hstring, 3> selectedResourceLanguages;
};

/// One symbolic-error-to-resource seam. MRT Core owns selection and fallback;
/// unknown future enum values map to the explicit generic resource triplet.
[[nodiscard]] ImageProcessingErrorText loadImageProcessingErrorText(
    domain::ImageProcessingErrorCode code,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap &resources,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext &context);
/// Native integration exceptions have no domain identity; do not invent one or
/// expose the exception message. Use the same explicitly generic resources.
[[nodiscard]] ImageProcessingErrorText loadGenericImageProcessingErrorText(
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceMap &resources,
    const winrt::Microsoft::Windows::ApplicationModel::Resources::ResourceContext &context);
} // namespace jpg_spinner::presentation
