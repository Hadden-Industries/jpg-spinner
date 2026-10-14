#pragma once

#include "Presentation.MainWindowViewModel.g.h"
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/storage/ImageFileTransactionEngine.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <optional>
#include <functional>
#include <chrono>
#include "Localization/ImageProcessingErrorText.h"

namespace winrt::JpgSpinner::Presentation::implementation
{
/// UI-thread-affine workflow projection. Composition retains the existing batch
/// coordinator and recovery engine; no codec or transaction implementation lives
/// here. Construction and every projected call require the supplied queue thread.
struct MainWindowViewModel : MainWindowViewModelT<MainWindowViewModel, winrt::non_agile>
{
    /// At most one representative structured failure per awaited operation,
    /// delivered on its owned MTA. No filename/display/metadata/native message
    /// is supplied. Observer failures never change processing/recovery outcomes.
    using OperationFailureDiagnosticObserver =
        std::function<void(const jpg_spinner::domain::ImageProcessingError &, std::chrono::milliseconds)>;
    MainWindowViewModel(Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
                        std::shared_ptr<const jpg_spinner::batch_processing::BatchProcessingCoordinator> coordinator,
                        std::shared_ptr<const jpg_spinner::storage::ImageFileTransactionEngine> recoveryEngine,
                        OperationFailureDiagnosticObserver diagnosticObserver = {});

    /// Stable workflow state and its permitted user actions; processing requires
    /// a coordinator-produced reviewed batch, never just a Boolean UI flag.
    Presentation::ImageProcessingExperienceState State() const;
    bool CanSelectSourceFolder() const;
    /// Analysis requires an actual selected collection in SourceSelection;
    /// displayed text or a previous review never grants that command.
    bool CanAnalyzeSelectedSource() const;
    bool CanBeginProcessing() const;
    bool CanCancelCurrentOperation() const;
    bool CanReturnToSourceSelection() const;
    /// Null means no known nonzero denominator; zero means known work not begun.
    Windows::Foundation::IReference<double> ProgressPercentage() const;
    bool IsProgressIndeterminate() const;
    double ProgressPercentageOrZero() const;
    /// Ordered coordinator facts in XAML's native inspectable collection
    /// protocol. Each item is ImageProcessingRowViewModel; no mutable interface
    /// is implemented, including through QueryInterface.
    Windows::Foundation::Collections::IVectorView<Windows::Foundation::IInspectable> Rows() const;
    /// Immutable traversal observations remain visible through review/results,
    /// without claiming they are successfully discovered image candidates.
    Windows::Foundation::Collections::IVectorView<Windows::Foundation::IInspectable> DiscoveryIssues() const;
    hstring BackupRootDisplayPath() const;
    /// User-facing folder label only; callers must retain the granted capability.
    hstring SourceFolderDisplayName() const;
    /// Coordinator-confirmed transformations; unsupported and already-normalized
    /// rows remain visible but do not contribute to this execution count.
    std::uint64_t EligibleFileCount() const;
    /// Explicit acknowledgment applies only to this reviewed row, never a later batch.
    void AcknowledgeEdgeTrimming(std::uint32_t rowIndex);
    std::uint64_t CompletedFileCount() const;
    std::uint64_t FailedFileCount() const;
    std::uint64_t CancelledFileCount() const;
    /// These closed policy projections preserve the native safe defaults.
    Presentation::OutputDisposition OutputDisposition() const;
    Presentation::TraversalScope TraversalScope() const;
    Presentation::EdgeHandlingPolicy EdgeHandlingPolicy() const;
    Presentation::OutputScanOrganization OutputScanOrganization() const;
    hstring ErrorTitle() const;
    hstring ErrorExplanation() const;
    hstring ErrorRemedy() const;
    bool HasError() const;
    /// Changes policy only for a selected source in an editable state. All
    /// projected values are validated before mutation. Changed choices discard
    /// review/acknowledgments and require fresh analysis; identical choices
    /// retain review. Neither operation starts transformation or transaction I/O.
    void SetProcessingOptions(Presentation::OutputDisposition disposition, Presentation::TraversalScope traversal,
                              Presentation::EdgeHandlingPolicy edgeHandling,
                              Presentation::OutputScanOrganization scanOrganization);
    /// Supply the granted collection and display context from the composition
    /// root. This is native-only: a displayed path grants no storage authority.
    void selectSource(jpg_spinner::batch_processing::BatchProcessingRequest request, hstring sourceFolderDisplayName,
                      hstring backupRootDisplayPath);
    /// Analyzes on an explicitly initialized MTA worker, then publishes review on
    /// the UI queue. Inputs are owned across suspension and no UI wait is used.
    Windows::Foundation::IAsyncAction AnalyzeAsync();
    /// Rejects a missing reviewed batch before scheduling any native work.
    Windows::Foundation::IAsyncAction BeginProcessingAsync();
    /// Recovery is serialized with analysis/processing and stays fail-closed.
    Windows::Foundation::IAsyncAction RecoverAsync();
    /// Requests cooperative cancellation; never claims that a completed commit was undone.
    void CancelCurrentOperation();
    /// Invalidates the reviewed artifact; forbidden during an active operation.
    void ReturnToSourceSelection();
    /// Standard observable binding subscription; handlers execute on the UI queue.
    event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler);
    void PropertyChanged(event_token const &token);

  private:
    void requireDispatcherThread() const;
    void notify(hstring const &propertyName);
    void changeState(Presentation::ImageProcessingExperienceState state);
    void applyProgress(jpg_spinner::batch_processing::BatchProcessingProgress const &progress);
    void showError(jpg_spinner::domain::ImageProcessingErrorCode code);
    void clearError();
    void showUnexpectedError();
    void observeFailure(const jpg_spinner::domain::ImageProcessingError &error,
                        std::chrono::steady_clock::time_point beganAt) const noexcept;
    void projectDiscoveryIssues(std::span<const jpg_spinner::domain::ImageSourceDiscoveryIssue> issues);
    /// Cancellation reaches native stop_source only. The private operation must
    /// still publish its terminal facts on the UI queue before its waiter ends.
    static Windows::Foundation::IAsyncAction awaitOperationAsync(Windows::Foundation::IAsyncAction operation,
                                                                 std::stop_source cancellation);
    Windows::Foundation::IAsyncAction analyzeAsync(jpg_spinner::batch_processing::BatchProcessingRequest request,
                                                   std::stop_source cancellation);
    Windows::Foundation::IAsyncAction processAsync(jpg_spinner::batch_processing::AnalyzedImageBatch batch,
                                                   std::stop_source cancellation);
    Windows::Foundation::IAsyncAction recoverAsync(std::stop_source cancellation);
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue_;
    std::shared_ptr<const jpg_spinner::batch_processing::BatchProcessingCoordinator> coordinator_;
    std::shared_ptr<const jpg_spinner::storage::ImageFileTransactionEngine> recoveryEngine_;
    const OperationFailureDiagnosticObserver diagnosticObserver_;
    event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged_;
    Presentation::ImageProcessingExperienceState state_{ImageProcessingExperienceState::SourceSelection};
    Windows::Foundation::Collections::IVectorView<Windows::Foundation::IInspectable> rows_{
        param::vector_view<Windows::Foundation::IInspectable>{std::vector<Windows::Foundation::IInspectable>{}}};
    std::optional<jpg_spinner::batch_processing::BatchProcessingRequest> selectedRequest_;
    Windows::Foundation::Collections::IVectorView<Windows::Foundation::IInspectable> discoveryIssues_{
        param::vector_view<Windows::Foundation::IInspectable>{std::vector<Windows::Foundation::IInspectable>{}}};
    std::optional<jpg_spinner::batch_processing::AnalyzedImageBatch> reviewedBatch_;
    std::stop_source cancellationSource_;
    bool recoveryActive_{};
    jpg_spinner::batch_processing::BatchProcessingProgress progress_{};
    hstring sourceFolderDisplayName_;
    hstring backupRootDisplayPath_;
    Microsoft::Windows::ApplicationModel::Resources::ResourceManager resourceManager_{nullptr};
    Microsoft::Windows::ApplicationModel::Resources::ResourceMap resources_{nullptr};
    Microsoft::Windows::ApplicationModel::Resources::ResourceContext resourceContext_{nullptr};
    jpg_spinner::presentation::ImageProcessingErrorText errorText_{};
};
} // namespace winrt::JpgSpinner::Presentation::implementation
