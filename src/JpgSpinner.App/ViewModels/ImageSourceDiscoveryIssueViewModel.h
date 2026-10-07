#pragma once
#include "Presentation.ImageSourceDiscoveryIssueViewModel.g.h"
#include <jpg_spinner/domain/ImageSourceCollection.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include "Localization/ImageProcessingErrorText.h"

namespace winrt::JpgSpinner::Presentation::implementation
{
/// Immutable UI-thread-affine traversal observation, separate from candidate
/// rows and their one-result-per-image ordinal contract. No storage capability.
struct ImageSourceDiscoveryIssueViewModel
    : ImageSourceDiscoveryIssueViewModelT<ImageSourceDiscoveryIssueViewModel, non_agile>
{
    ImageSourceDiscoveryIssueViewModel(Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
                                       jpg_spinner::domain::ImageSourceDiscoveryIssue const &issue,
                                       Microsoft::Windows::ApplicationModel::Resources::ResourceMap const &resources,
                                       Microsoft::Windows::ApplicationModel::Resources::ResourceContext const &context);
    /// Original relative display context only; never used as file authority.
    hstring RelativePath() const;
    Presentation::ImageSourceDiscoveryIssueKind Kind() const;
    /// Empty for deliberate exclusions without a domain error. The kind remains
    /// explicit so the shell can distinguish exclusion from failed access.
    hstring ErrorTitle() const;
    hstring ErrorExplanation() const;
    hstring ErrorRemedy() const;

  private:
    void requireDispatcherThread() const;
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue_;
    hstring relativePath_;
    Presentation::ImageSourceDiscoveryIssueKind kind_;
    jpg_spinner::presentation::ImageProcessingErrorText errorText_{};
};
} // namespace winrt::JpgSpinner::Presentation::implementation
