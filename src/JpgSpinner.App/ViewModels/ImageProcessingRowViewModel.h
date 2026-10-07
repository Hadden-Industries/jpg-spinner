#pragma once
#include "Presentation.ImageProcessingRowViewModel.g.h"
#include <jpg_spinner/batch/AnalyzedImageBatch.h>
#include <jpg_spinner/batch/BatchFileOutcome.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include "Localization/ImageProcessingErrorText.h"

namespace winrt::JpgSpinner::Presentation::implementation
{
/// Display-only projection of immutable coordinator facts. It carries neither
/// encoded bytes nor source/commit capabilities. All calls require its UI queue.
struct ImageProcessingRowViewModel : ImageProcessingRowViewModelT<ImageProcessingRowViewModel, non_agile>
{
    ImageProcessingRowViewModel(Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
                                const jpg_spinner::batch_processing::BatchFileAnalysis &analysis,
                                Microsoft::Windows::ApplicationModel::Resources::ResourceMap resources,
                                Microsoft::Windows::ApplicationModel::Resources::ResourceContext resourceContext);
    /// Only the leaf display name is shown; full paths do not enter error prose.
    hstring DisplayName() const;
    /// Eligibility requires a successful non-identity plan; an extension alone is insufficient.
    bool IsEligibleForProcessing() const;
    /// Source-space discarded extents from the approved planner, not output geometry.
    std::uint64_t DiscardedSourceRightEdgeWidthInPixels() const;
    std::uint64_t DiscardedSourceBottomEdgeHeightInPixels() const;
    bool IsEdgeTrimmingAcknowledged() const;
    /// Native-only mutation; the owning model checks Review state first.
    void acknowledgeEdgeTrimming();
    Presentation::ImageProcessingRowOutcome Outcome() const;
    hstring ErrorTitle() const;
    hstring ErrorExplanation() const;
    hstring ErrorRemedy() const;
    /// Projects the coordinator's terminal observation without assuming a commit.
    void applyResult(const jpg_spinner::batch_processing::BatchFileProcessingResult &result);
    /// Native observable-property subscriptions; notifications stay on the UI queue.
    event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler);
    void PropertyChanged(event_token const &token);

  private:
    void requireDispatcherThread() const;
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue_;
    hstring displayName_;
    bool isEligibleForProcessing_;
    std::uint64_t discardedSourceRightEdgeWidthInPixels_{};
    std::uint64_t discardedSourceBottomEdgeHeightInPixels_{};
    bool isEdgeTrimmingAcknowledged_{};
    Presentation::ImageProcessingRowOutcome outcome_{ImageProcessingRowOutcome::Pending};
    Microsoft::Windows::ApplicationModel::Resources::ResourceMap resources_;
    Microsoft::Windows::ApplicationModel::Resources::ResourceContext resourceContext_;
    jpg_spinner::presentation::ImageProcessingErrorText errorText_{};
    event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged_;
};
} // namespace winrt::JpgSpinner::Presentation::implementation
