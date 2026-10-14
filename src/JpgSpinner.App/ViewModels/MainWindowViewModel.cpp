#include "pch.h"
#include "ViewModels/MainWindowViewModel.h"
#include "Localization/UnsignedIntegerText.h"
#include "ViewModels/ImageProcessingRowViewModel.h"
#include "ViewModels/ImageSourceDiscoveryIssueViewModel.h"
#include "Presentation.MainWindowViewModel.g.cpp"
#include <algorithm>
#include <roapi.h>
#include <wil/cppwinrt_helpers.h>
#include <wil/resource.h>

namespace winrt::JpgSpinner::Presentation::implementation
{
namespace
{
template <typename Files> const jpg_spinner::domain::ImageProcessingError *firstFileFailure(const Files &files)
{
    for (const auto &file : files)
        if (file.error)
            return &*file.error;
    return nullptr;
}
} // namespace
MainWindowViewModel::MainWindowViewModel(
    Microsoft::UI::Dispatching::DispatcherQueue dispatcherQueue,
    std::shared_ptr<const jpg_spinner::batch_processing::BatchProcessingCoordinator> coordinator,
    std::shared_ptr<const jpg_spinner::storage::ImageFileTransactionEngine> recoveryEngine,
    OperationFailureDiagnosticObserver diagnosticObserver)
    : dispatcherQueue_(std::move(dispatcherQueue)), coordinator_(std::move(coordinator)),
      recoveryEngine_(std::move(recoveryEngine)), diagnosticObserver_(std::move(diagnosticObserver))
{
    if (!dispatcherQueue_ || !coordinator_ || !recoveryEngine_)
        throw hresult_invalid_argument();
    requireDispatcherThread();
    // Explicit PRI path works for the packaged app and the native unpackaged
    // verifier. MRT Core, not presentation, owns language selection/fallback.
    resourceManager_ = Microsoft::Windows::ApplicationModel::Resources::ResourceManager{
        Microsoft::Windows::ApplicationModel::Resources::ResourceLoader::GetDefaultResourceFilePath()};
    resources_ = resourceManager_.MainResourceMap().GetSubtree(L"Resources");
    resourceContext_ = resourceManager_.CreateResourceContext();
}
void MainWindowViewModel::requireDispatcherThread() const
{
    if (!dispatcherQueue_.HasThreadAccess())
        throw hresult_wrong_thread();
}
Presentation::ImageProcessingExperienceState MainWindowViewModel::State() const
{
    requireDispatcherThread();
    return state_;
}
bool MainWindowViewModel::CanSelectSourceFolder() const
{
    requireDispatcherThread();
    return state_ == ImageProcessingExperienceState::SourceSelection ||
           state_ == ImageProcessingExperienceState::Review || state_ == ImageProcessingExperienceState::Results;
}
bool MainWindowViewModel::CanAnalyzeSelectedSource() const
{
    requireDispatcherThread();
    return state_ == ImageProcessingExperienceState::SourceSelection && selectedRequest_.has_value();
}
hstring MainWindowViewModel::SourceFolderDisplayName() const
{
    requireDispatcherThread();
    return sourceFolderDisplayName_;
}
std::uint64_t MainWindowViewModel::EligibleFileCount() const
{
    requireDispatcherThread();
    return progress_.eligible;
}
bool MainWindowViewModel::CanBeginProcessing() const
{
    requireDispatcherThread();
    if (state_ != ImageProcessingExperienceState::Review || !reviewedBatch_ || !reviewedBatch_->discoveryComplete())
        return false;
    if (selectedRequest_->outputDisposition ==
            jpg_spinner::domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup &&
        backupRootDisplayPath_.empty())
        return false;
    if (std::any_of(rows_.begin(), rows_.end(), [](Windows::Foundation::IInspectable const &item) {
            const auto row = item.as<Presentation::ImageProcessingRowViewModel>();
            return row.IsEligibleForProcessing() &&
                   (row.DiscardedSourceRightEdgeWidthInPixels() != 0 ||
                    row.DiscardedSourceBottomEdgeHeightInPixels() != 0) &&
                   !row.IsEdgeTrimmingAcknowledged();
        }))
        return false;
    return std::any_of(rows_.begin(), rows_.end(), [](Windows::Foundation::IInspectable const &item) {
        return item.as<Presentation::ImageProcessingRowViewModel>().IsEligibleForProcessing();
    });
}
bool MainWindowViewModel::CanCancelCurrentOperation() const
{
    requireDispatcherThread();
    return state_ == ImageProcessingExperienceState::Analysis || state_ == ImageProcessingExperienceState::Processing;
}
std::uint64_t MainWindowViewModel::CompletedFileCount() const
{
    requireDispatcherThread();
    return progress_.completed;
}
std::uint64_t MainWindowViewModel::FailedFileCount() const
{
    requireDispatcherThread();
    return progress_.failed;
}
std::uint64_t MainWindowViewModel::CancelledFileCount() const
{
    requireDispatcherThread();
    return progress_.cancelled;
}
hstring MainWindowViewModel::EligibleFileCountText() const
{
    return jpg_spinner::presentation::formatUnsignedInteger(EligibleFileCount());
}
hstring MainWindowViewModel::CompletedFileCountText() const
{
    return jpg_spinner::presentation::formatUnsignedInteger(CompletedFileCount());
}
hstring MainWindowViewModel::FailedFileCountText() const
{
    return jpg_spinner::presentation::formatUnsignedInteger(FailedFileCount());
}
hstring MainWindowViewModel::CancelledFileCountText() const
{
    return jpg_spinner::presentation::formatUnsignedInteger(CancelledFileCount());
}
Presentation::OutputDisposition MainWindowViewModel::OutputDisposition() const
{
    requireDispatcherThread();
    const auto value = selectedRequest_ ? selectedRequest_->outputDisposition
                                        : jpg_spinner::domain::OutputDisposition::CreateCorrectedCopy;
    switch (value)
    {
    case jpg_spinner::domain::OutputDisposition::CreateCorrectedCopy:
        return Presentation::OutputDisposition::CreateCorrectedCopy;
    case jpg_spinner::domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup:
        return Presentation::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
    default:
        throw hresult_invalid_argument();
    }
}
Presentation::TraversalScope MainWindowViewModel::TraversalScope() const
{
    requireDispatcherThread();
    const auto value =
        selectedRequest_ ? selectedRequest_->traversalScope : jpg_spinner::domain::TraversalScope::SelectedFolderOnly;
    switch (value)
    {
    case jpg_spinner::domain::TraversalScope::SelectedFolderOnly:
        return Presentation::TraversalScope::SelectedFolderOnly;
    case jpg_spinner::domain::TraversalScope::SelectedFolderAndDescendants:
        return Presentation::TraversalScope::SelectedFolderAndDescendants;
    default:
        throw hresult_invalid_argument();
    }
}
Presentation::EdgeHandlingPolicy MainWindowViewModel::EdgeHandlingPolicy() const
{
    requireDispatcherThread();
    const auto value = selectedRequest_ ? selectedRequest_->edgeHandlingPolicy
                                        : jpg_spinner::domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform;
    switch (value)
    {
    case jpg_spinner::domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform:
        return Presentation::EdgeHandlingPolicy::RequirePerfectCoefficientTransform;
    case jpg_spinner::domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits:
        return Presentation::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
    default:
        throw hresult_invalid_argument();
    }
}
Presentation::OutputScanOrganization MainWindowViewModel::OutputScanOrganization() const
{
    requireDispatcherThread();
    const auto value = selectedRequest_ ? selectedRequest_->outputScanOrganization
                                        : jpg_spinner::domain::OutputScanOrganization::PreserveSource;
    switch (value)
    {
    case jpg_spinner::domain::OutputScanOrganization::PreserveSource:
        return Presentation::OutputScanOrganization::PreserveSource;
    case jpg_spinner::domain::OutputScanOrganization::SequentialDct:
        return Presentation::OutputScanOrganization::SequentialDct;
    case jpg_spinner::domain::OutputScanOrganization::ProgressiveDct:
        return Presentation::OutputScanOrganization::ProgressiveDct;
    default:
        throw hresult_invalid_argument();
    }
}
hstring MainWindowViewModel::ErrorTitle() const
{
    requireDispatcherThread();
    return errorText_.title;
}
hstring MainWindowViewModel::ErrorExplanation() const
{
    requireDispatcherThread();
    return errorText_.explanation;
}
hstring MainWindowViewModel::ErrorRemedy() const
{
    requireDispatcherThread();
    return errorText_.remedy;
}
bool MainWindowViewModel::HasError() const
{
    requireDispatcherThread();
    return !errorText_.title.empty();
}
bool MainWindowViewModel::IsProgressIndeterminate() const
{
    return !ProgressPercentage();
}
double MainWindowViewModel::ProgressPercentageOrZero() const
{
    const auto percentage = ProgressPercentage();
    // WinUI ProgressBar requires a double. Its separate indeterminate flag
    // preserves the distinction between unknown work and known zero progress.
    return percentage ? percentage.Value() : 0.0;
}
void MainWindowViewModel::showError(jpg_spinner::domain::ImageProcessingErrorCode code)
{
    errorText_ = jpg_spinner::presentation::loadImageProcessingErrorText(code, resources_, resourceContext_);
    for (auto const *property : {L"ErrorTitle", L"ErrorExplanation", L"ErrorRemedy", L"HasError"})
        notify(property);
}
void MainWindowViewModel::clearError()
{
    errorText_ = {};
    for (auto const *property : {L"ErrorTitle", L"ErrorExplanation", L"ErrorRemedy", L"HasError"})
        notify(property);
}
void MainWindowViewModel::showUnexpectedError()
{
    errorText_ = jpg_spinner::presentation::loadGenericImageProcessingErrorText(resources_, resourceContext_);
    for (auto const *property : {L"ErrorTitle", L"ErrorExplanation", L"ErrorRemedy", L"HasError"})
        notify(property);
}
bool MainWindowViewModel::CanReturnToSourceSelection() const
{
    requireDispatcherThread();
    return state_ == ImageProcessingExperienceState::Review || state_ == ImageProcessingExperienceState::Results;
}
Windows::Foundation::IReference<double> MainWindowViewModel::ProgressPercentage() const
{
    requireDispatcherThread();
    if (!progress_.discoveryComplete || progress_.discovered == 0)
        return nullptr;
    const auto finished =
        state_ == ImageProcessingExperienceState::Analysis || state_ == ImageProcessingExperienceState::Review
            ? progress_.analyzed
            : progress_.completed;
    return box_value(100.0 * static_cast<double>(finished) / static_cast<double>(progress_.discovered))
        .as<Windows::Foundation::IReference<double>>();
}
Windows::Foundation::Collections::IVectorView<Windows::Foundation::IInspectable> MainWindowViewModel::Rows() const
{
    requireDispatcherThread();
    return rows_;
}
hstring MainWindowViewModel::BackupRootDisplayPath() const
{
    requireDispatcherThread();
    return backupRootDisplayPath_;
}
Windows::Foundation::Collections::IVectorView<Windows::Foundation::IInspectable> MainWindowViewModel::DiscoveryIssues()
    const
{
    requireDispatcherThread();
    return discoveryIssues_;
}
void MainWindowViewModel::projectDiscoveryIssues(std::span<const jpg_spinner::domain::ImageSourceDiscoveryIssue> issues)
{
    requireDispatcherThread();
    std::vector<Windows::Foundation::IInspectable> projectedIssues;
    projectedIssues.reserve(issues.size());
    for (auto const &issue : issues)
        projectedIssues.push_back(
            make<ImageSourceDiscoveryIssueViewModel>(dispatcherQueue_, issue, resources_, resourceContext_));
    discoveryIssues_ = param::vector_view<Windows::Foundation::IInspectable>{std::move(projectedIssues)};
    notify(L"DiscoveryIssues");
}
void MainWindowViewModel::AcknowledgeEdgeTrimming(std::uint32_t rowIndex)
{
    requireDispatcherThread();
    if (state_ != ImageProcessingExperienceState::Review)
        throw hresult_illegal_method_call();
    auto row = rows_.GetAt(rowIndex);
    get_self<ImageProcessingRowViewModel>(row.as<Presentation::ImageProcessingRowViewModel>())
        ->acknowledgeEdgeTrimming();
    notify(L"CanBeginProcessing");
}
void MainWindowViewModel::notify(hstring const &propertyName)
{
    propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{propertyName});
}
void MainWindowViewModel::changeState(Presentation::ImageProcessingExperienceState state)
{
    const bool stateChanged = state_ != state;
    state_ = state;
    if (stateChanged)
        notify(L"State");
    for (auto const *property :
         {L"CanSelectSourceFolder", L"CanAnalyzeSelectedSource", L"CanBeginProcessing", L"CanCancelCurrentOperation",
          L"CanReturnToSourceSelection", L"ProgressPercentage", L"IsProgressIndeterminate", L"ProgressPercentageOrZero",
          L"EligibleFileCount", L"CompletedFileCount", L"FailedFileCount", L"CancelledFileCount",
          L"EligibleFileCountText", L"CompletedFileCountText", L"FailedFileCountText", L"CancelledFileCountText"})
        notify(property);
}
void MainWindowViewModel::selectSource(jpg_spinner::batch_processing::BatchProcessingRequest request,
                                       hstring sourceFolderDisplayName, hstring backupRootDisplayPath)
{
    requireDispatcherThread();
    if (!CanSelectSourceFolder())
        throw hresult_illegal_method_call();
    if (!request.sourceCollection)
        throw hresult_invalid_argument();
    reviewedBatch_.reset();
    rows_ = param::vector_view<Windows::Foundation::IInspectable>{std::vector<Windows::Foundation::IInspectable>{}};
    notify(L"Rows");
    selectedRequest_.emplace(std::move(request));
    projectDiscoveryIssues({});
    for (auto const *property :
         {L"OutputDisposition", L"TraversalScope", L"EdgeHandlingPolicy", L"OutputScanOrganization"})
        notify(property);
    clearError();
    sourceFolderDisplayName_ = std::move(sourceFolderDisplayName);
    notify(L"SourceFolderDisplayName");
    backupRootDisplayPath_ = std::move(backupRootDisplayPath);
    notify(L"BackupRootDisplayPath");
    progress_ = {};
    changeState(ImageProcessingExperienceState::SourceSelection);
}
void MainWindowViewModel::applyProgress(jpg_spinner::batch_processing::BatchProcessingProgress const &progress)
{
    requireDispatcherThread();
    progress_ = progress;
    notify(L"ProgressPercentage");
    notify(L"IsProgressIndeterminate");
    notify(L"ProgressPercentageOrZero");
    notify(L"EligibleFileCount");
    notify(L"CompletedFileCount");
    notify(L"FailedFileCount");
    notify(L"CancelledFileCount");
    notify(L"EligibleFileCountText");
    notify(L"CompletedFileCountText");
    notify(L"FailedFileCountText");
    notify(L"CancelledFileCountText");
}
Windows::Foundation::IAsyncAction MainWindowViewModel::AnalyzeAsync()
{
    requireDispatcherThread();
    if (!CanAnalyzeSelectedSource())
        throw hresult_illegal_method_call();
    cancellationSource_ = std::stop_source{};
    progress_ = {};
    clearError();
    changeState(ImageProcessingExperienceState::Analysis);
    return awaitOperationAsync(analyzeAsync(*selectedRequest_, cancellationSource_), cancellationSource_);
}
void MainWindowViewModel::SetProcessingOptions(Presentation::OutputDisposition disposition,
                                               Presentation::TraversalScope traversal,
                                               Presentation::EdgeHandlingPolicy edgeHandling,
                                               Presentation::OutputScanOrganization scanOrganization)
{
    requireDispatcherThread();
    if (!CanSelectSourceFolder() || !selectedRequest_)
        throw hresult_illegal_method_call();
    // Build a complete native request before publishing anything. Do not cast
    // between unrelated enum contracts or partially mutate a live review when
    // a later projected argument is unknown. The granted collection, limits and
    // batch identity remain the original capabilities/facts, not display paths.
    auto request = *selectedRequest_;
    switch (disposition)
    {
    case Presentation::OutputDisposition::CreateCorrectedCopy:
        request.outputDisposition = jpg_spinner::domain::OutputDisposition::CreateCorrectedCopy;
        break;
    case Presentation::OutputDisposition::ReplaceOriginalWithVerifiedBackup:
        request.outputDisposition = jpg_spinner::domain::OutputDisposition::ReplaceOriginalWithVerifiedBackup;
        break;
    default:
        throw hresult_invalid_argument();
    }
    switch (traversal)
    {
    case Presentation::TraversalScope::SelectedFolderOnly:
        request.traversalScope = jpg_spinner::domain::TraversalScope::SelectedFolderOnly;
        break;
    case Presentation::TraversalScope::SelectedFolderAndDescendants:
        request.traversalScope = jpg_spinner::domain::TraversalScope::SelectedFolderAndDescendants;
        break;
    default:
        throw hresult_invalid_argument();
    }
    switch (edgeHandling)
    {
    case Presentation::EdgeHandlingPolicy::RequirePerfectCoefficientTransform:
        request.edgeHandlingPolicy = jpg_spinner::domain::EdgeHandlingPolicy::RequirePerfectCoefficientTransform;
        break;
    case Presentation::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits:
        request.edgeHandlingPolicy = jpg_spinner::domain::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits;
        break;
    default:
        throw hresult_invalid_argument();
    }
    switch (scanOrganization)
    {
    case Presentation::OutputScanOrganization::PreserveSource:
        request.outputScanOrganization = jpg_spinner::domain::OutputScanOrganization::PreserveSource;
        break;
    case Presentation::OutputScanOrganization::SequentialDct:
        request.outputScanOrganization = jpg_spinner::domain::OutputScanOrganization::SequentialDct;
        break;
    case Presentation::OutputScanOrganization::ProgressiveDct:
        request.outputScanOrganization = jpg_spinner::domain::OutputScanOrganization::ProgressiveDct;
        break;
    default:
        throw hresult_invalid_argument();
    }
    if (request.outputDisposition == selectedRequest_->outputDisposition &&
        request.traversalScope == selectedRequest_->traversalScope &&
        request.edgeHandlingPolicy == selectedRequest_->edgeHandlingPolicy &&
        request.outputScanOrganization == selectedRequest_->outputScanOrganization)
        return;
    // Reuse the one source/review invalidation path. A reanalysis is mandatory;
    // changing a control must never repurpose the already reviewed batch.
    selectSource(std::move(request), sourceFolderDisplayName_, backupRootDisplayPath_);
}
Windows::Foundation::IAsyncAction MainWindowViewModel::awaitOperationAsync(Windows::Foundation::IAsyncAction operation,
                                                                           std::stop_source cancellation)
{
    // Microsoft's native cancellation-callback pattern integrates stop_source.
    // Do not enable cancellation propagation: C++/WinRT throws at the next await
    // of a cancelled action, which would skip the private worker's mandatory UI
    // finalization (including commit outcomes and the recovery retry gate).
    auto asyncCancellation = co_await get_cancellation_token();
    asyncCancellation.callback([cancellation]() mutable { cancellation.request_stop(); });
    co_await operation;
}
Windows::Foundation::IAsyncAction MainWindowViewModel::analyzeAsync(
    jpg_spinner::batch_processing::BatchProcessingRequest request, std::stop_source cancellation)
{
    auto lifetime = get_strong(); // Own the model before the first suspension.
    auto queue = dispatcherQueue_;
    auto coordinator = coordinator_;
    auto weak = get_weak();
    const auto beganAt = std::chrono::steady_clock::now();
    co_await resume_background();
    // Thread-pool scheduling is not a storage apartment contract. Establish and
    // balance MTA explicitly within this synchronous native-operation segment.
    std::optional<jpg_spinner::domain::ImageProcessingResult<jpg_spinner::batch_processing::AnalyzedImageBatch>> result;
    try
    {
        auto apartmentLifetime = wil::RoInitialize(RO_INIT_MULTITHREADED);
        result.emplace(coordinator->analyze(request, cancellation.get_token(), [queue, weak](auto const &progress) {
            queue.TryEnqueue([weak, progress] {
                if (auto model = weak.get())
                    model->applyProgress(progress);
            });
        }));
        if (const auto error = result->errorIfPresent())
            observeFailure(*error, beganAt);
        else if (const auto fileFailure = firstFileFailure(result->valueIfPresent()->files()))
            observeFailure(*fileFailure, beganAt);
    }
    catch (...)
    {
        // A native integration exception is not a fabricated domain failure.
        // Return to the UI queue before publishing safe generic text and state.
    }
    // WIL detects queue shutdown and reports ERROR_NO_TASK_QUEUE instead of
    // silently continuing on an arbitrary worker thread.
    co_await wil::resume_foreground(queue);
    // Only the queue thread owns model state and observable collections. The
    // synchronous worker owns JPEG buffers entirely inside the coordinator.
    if (!result || result->errorIfPresent() || cancellation.stop_requested())
    {
        if (!result)
            showUnexpectedError();
        else if (auto error = result->errorIfPresent())
            showError(error->code);
        progress_ = {};
        changeState(ImageProcessingExperienceState::SourceSelection);
        co_return;
    }
    // Move-only analysis cannot be reused after execution. Read-only projection
    // is separate from the coordinator's retained capability/revision artifact.
    reviewedBatch_.emplace(std::move(*result->valueIfPresent()));
    projectDiscoveryIssues(reviewedBatch_->discoveryIssues());
    std::vector<Windows::Foundation::IInspectable> projectedRows;
    projectedRows.reserve(reviewedBatch_->files().size());
    for (auto const &file : reviewedBatch_->files())
        projectedRows.push_back(make<ImageProcessingRowViewModel>(queue, file, resources_, resourceContext_));
    // C++/WinRT owns the rvalue container. A scoped borrowed view or a vector
    // supporting mutable QueryInterface would let callers alter the review rows.
    rows_ = param::vector_view<Windows::Foundation::IInspectable>{std::move(projectedRows)};
    notify(L"Rows");
    changeState(ImageProcessingExperienceState::Review);
}
void MainWindowViewModel::ReturnToSourceSelection()
{
    requireDispatcherThread();
    if (!CanReturnToSourceSelection())
        throw hresult_illegal_method_call();
    reviewedBatch_.reset();
    projectDiscoveryIssues({});
    rows_ = param::vector_view<Windows::Foundation::IInspectable>{std::vector<Windows::Foundation::IInspectable>{}};
    notify(L"Rows");
    progress_ = {};
    clearError();
    changeState(ImageProcessingExperienceState::SourceSelection);
}
Windows::Foundation::IAsyncAction MainWindowViewModel::BeginProcessingAsync()
{
    requireDispatcherThread();
    if (!CanBeginProcessing())
        throw hresult_illegal_method_call();
    cancellationSource_ = std::stop_source{};
    clearError();
    auto batch = std::move(*reviewedBatch_);
    reviewedBatch_.reset(); // A reviewed capability is transferred exactly once.
    progress_.completed = 0;
    changeState(ImageProcessingExperienceState::Processing);
    return awaitOperationAsync(processAsync(std::move(batch), cancellationSource_), cancellationSource_);
}
Windows::Foundation::IAsyncAction MainWindowViewModel::processAsync(
    jpg_spinner::batch_processing::AnalyzedImageBatch batch, std::stop_source cancellation)
{
    auto lifetime = get_strong();
    auto queue = dispatcherQueue_;
    auto coordinator = coordinator_;
    auto weak = get_weak();
    const auto beganAt = std::chrono::steady_clock::now();
    co_await resume_background();
    std::optional<jpg_spinner::domain::ImageProcessingResult<jpg_spinner::batch_processing::BatchProcessingSummary>>
        result;
    try
    {
        auto apartmentLifetime = wil::RoInitialize(RO_INIT_MULTITHREADED);
        result.emplace(
            coordinator->execute(std::move(batch), cancellation.get_token(), [queue, weak](auto const &progress) {
                queue.TryEnqueue([weak, progress] {
                    if (auto model = weak.get())
                        model->applyProgress(progress);
                });
            }));
        if (const auto error = result->errorIfPresent())
            observeFailure(*error, beganAt);
        else if (const auto fileFailure = firstFileFailure(result->valueIfPresent()->fileResults))
            observeFailure(*fileFailure, beganAt);
    }
    catch (...)
    {
        // Without a terminal summary, earlier commits cannot be inferred away.
        // Preserve all observations and require native recovery on the UI side.
    }
    co_await wil::resume_foreground(queue);
    if (auto summary = result ? result->valueIfPresent() : nullptr)
    {
        progress_ = summary->finalProgress;
        projectDiscoveryIssues(summary->discoveryIssues);
        for (std::uint32_t index = 0; index < rows_.Size(); ++index)
            get_self<ImageProcessingRowViewModel>(rows_.GetAt(index).as<Presentation::ImageProcessingRowViewModel>())
                ->applyResult(summary->fileResults.at(index));
        // A cancellation request after a successful commit cannot rewrite its
        // terminal outcome. Only uncertain transaction facts require recovery.
        changeState(summary->transactionIntegrityUncertain ? ImageProcessingExperienceState::RecoveryRequired
                                                           : ImageProcessingExperienceState::Results);
    }
    else
    {
        if (result)
            showError(result->errorIfPresent()->code);
        else
            showUnexpectedError();
        // No trustworthy aggregate observation: do not start another transaction
        // until the injected recovery capability has reconciled retained records.
        changeState(ImageProcessingExperienceState::RecoveryRequired);
    }
}
void MainWindowViewModel::CancelCurrentOperation()
{
    requireDispatcherThread();
    if (!CanCancelCurrentOperation())
        throw hresult_illegal_method_call();
    cancellationSource_.request_stop();
}
Windows::Foundation::IAsyncAction MainWindowViewModel::RecoverAsync()
{
    requireDispatcherThread();
    if (recoveryActive_ || (state_ != ImageProcessingExperienceState::SourceSelection &&
                            state_ != ImageProcessingExperienceState::RecoveryRequired))
        throw hresult_illegal_method_call();
    recoveryActive_ = true;
    cancellationSource_ = std::stop_source{};
    clearError();
    reviewedBatch_.reset();
    changeState(ImageProcessingExperienceState::RecoveryRequired);
    return awaitOperationAsync(recoverAsync(cancellationSource_), cancellationSource_);
}
Windows::Foundation::IAsyncAction MainWindowViewModel::recoverAsync(std::stop_source cancellation)
{
    auto lifetime = get_strong();
    auto queue = dispatcherQueue_;
    auto recovery = recoveryEngine_;
    const auto beganAt = std::chrono::steady_clock::now();
    co_await resume_background();
    std::optional<jpg_spinner::domain::ImageProcessingResult<jpg_spinner::storage::ImageFileTransactionRecoverySummary>>
        result;
    try
    {
        auto apartmentLifetime = wil::RoInitialize(RO_INIT_MULTITHREADED);
        result.emplace(recovery->recoverIncompleteTransactions(cancellation.get_token()));
        if (const auto error = result->errorIfPresent())
            observeFailure(*error, beganAt);
    }
    catch (...)
    {
        // Do not leak native exception text or leave the retry gate busy forever.
        // A missing result still leaves recovery unresolved.
    }
    co_await wil::resume_foreground(queue);
    recoveryActive_ = false;
    if (!result || result->errorIfPresent())
    {
        if (result)
            showError(result->errorIfPresent()->code);
        else
            showUnexpectedError();
        changeState(ImageProcessingExperienceState::RecoveryRequired);
    }
    else
    {
        rows_ = param::vector_view<Windows::Foundation::IInspectable>{std::vector<Windows::Foundation::IInspectable>{}};
        projectDiscoveryIssues({});
        notify(L"Rows");
        progress_ = {};
        changeState(ImageProcessingExperienceState::SourceSelection);
    }
}
void MainWindowViewModel::observeFailure(const jpg_spinner::domain::ImageProcessingError &error,
                                         const std::chrono::steady_clock::time_point beganAt) const noexcept
{
    if (!diagnosticObserver_)
        return;
    try
    {
        diagnosticObserver_(
            error, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beganAt));
    }
    catch (...)
    {
        // Diagnostic failure cannot replace a real coordinator/recovery result
        // or skip its mandatory UI finalization. Do not log recursively.
    }
}
event_token MainWindowViewModel::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler)
{
    requireDispatcherThread();
    return propertyChanged_.add(handler);
}
void MainWindowViewModel::PropertyChanged(event_token const &token)
{
    requireDispatcherThread();
    propertyChanged_.remove(token);
}
} // namespace winrt::JpgSpinner::Presentation::implementation
