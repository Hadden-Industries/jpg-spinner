#include "pch.h"

#include "MainWindow.xaml.h"
#include "ViewModels/MainWindowViewModel.h"
#include "Composition/SelectedFolderBatchComposition.h"
#include "Composition/StartupRecoveryComposition.h"
#include "Localization/ImageProcessingErrorText.h"
#include "Localization/UnsignedIntegerText.h"
#include "Diagnostics/DiagnosticLogExport.h"
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Storage.Provider.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>
#include <roapi.h>
#include <wil/cppwinrt_helpers.h>
#include <wil/resource.h>
#include <bit>
#include <cmath>
#include <optional>

// MIDL emits the activation-factory entry point in this generated source.
// Include it in the owning runtime class translation unit, as prescribed by
// the C++/WinRT project template, so module.g.cpp can resolve the factory.
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace winrt::JpgSpinner::implementation
{
MainWindow::MainWindow()
{
    InitializeComponent();
    // The native TitleBar owns caption/drag regions, RTL and material fallback.
    ExtendsContentIntoTitleBar(true);
    SetTitleBar(AppTitleBar());
    closingEventSource_ = AppWindow();
    closingEventToken_ = closingEventSource_.Closing([weak = get_weak()](auto const &, auto const &arguments) {
        if (auto window = weak.get())
            window->handleClosing(arguments);
    });
    WorkflowContent().Loaded([weak = get_weak()](auto const &, auto const &) {
        if (auto window = weak.get())
        {
            window->updatePreferredMinimumWindowSize();
            if (!window->startupRecoveryStarted_)
            {
                window->startupRecoveryStarted_ = true;
                window->activeOperation_ = window->recoverStartupAsync().as<Windows::Foundation::IAsyncInfo>();
            }
            // XamlRoot reports the current display scale, including monitor
            // changes. Keep the effective-pixel minimum without a WndProc
            // replacement or a resize/SizeChanged feedback loop.
            window->xamlRootChangedSubscription_ =
                window->WorkflowContent().XamlRoot().Changed(auto_revoke, [weak](auto const &, auto const &) {
                    if (auto current = weak.get())
                        current->updatePreferredMinimumWindowSize();
                });
        }
    });
    coordinator_ = std::make_shared<jpg_spinner::batch_processing::BatchProcessingCoordinator>(
        std::make_shared<jpg_spinner::jpeg::LibJpegTurboTransformationEngine>());
}
void MainWindow::updatePreferredMinimumWindowSize()
{
    requireUiThread();
    // AppWindow coordinates are physical pixels; XAML's layout contract is
    // effective pixels. Round upward so fractional display scales cannot make
    // the native minimum smaller than 360 by 480 effective pixels.
    // https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.window.sizechanged
    const auto scale = WorkflowContent().XamlRoot().RasterizationScale();
    const auto presenter = AppWindow().Presenter().as<Microsoft::UI::Windowing::OverlappedPresenter>();
    presenter.PreferredMinimumWidth(static_cast<int>(std::ceil(360 * scale)));
    presenter.PreferredMinimumHeight(static_cast<int>(std::ceil(480 * scale)));
}
MainWindow::~MainWindow()
{
    // AppWindow is a native event source without weak-reference support; use
    // Microsoft's event-token removal rather than automatic weak revocation.
    // https://learn.microsoft.com/windows/uwp/cpp-and-winrt-apis/handle-events
    if (closingEventSource_ && closingEventToken_.value != 0)
        closingEventSource_.Closing(closingEventToken_);
}
void MainWindow::requireUiThread() const
{
    if (!DispatcherQueue().HasThreadAccess())
        throw hresult_wrong_thread();
}
Presentation::MainWindowViewModel MainWindow::ViewModel() const
{
    requireUiThread();
    return viewModel_;
}
bool MainWindow::CanPickSourceFolder() const
{
    requireUiThread();
    return !closeAdmission_.closeRequested() && !startupRecoveryPending_ && !startupRecoveryError_ && !pickerActive_ &&
           !operationEventActive_ && (!viewModel_ || viewModel_.CanSelectSourceFolder());
}
bool MainWindow::IsWorkflowInteractionEnabled() const
{
    requireUiThread();
    return !closeAdmission_.closeRequested() && !startupRecoveryPending_ && !pickerActive_;
}
bool MainWindow::CanRetryRecovery() const
{
    requireUiThread();
    return !closeAdmission_.closeRequested() && !startupRecoveryPending_ && !pickerActive_ && !operationEventActive_ &&
           (startupRecoveryError_.has_value() ||
            (viewModel_ && viewModel_.State() == Presentation::ImageProcessingExperienceState::RecoveryRequired));
}
bool MainWindow::IsStartupRecoveryPending() const
{
    requireUiThread();
    return startupRecoveryPending_;
}
bool MainWindow::HasStartupRecoveryError() const
{
    requireUiThread();
    return startupRecoveryError_.has_value();
}
hstring MainWindow::StartupRecoveryErrorMessage() const
{
    requireUiThread();
    if (!startupRecoveryError_)
        return {};
    using namespace Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    return jpg_spinner::presentation::loadStartupRecoveryErrorMessage(
        startupRecoveryError_->code, manager.MainResourceMap().GetSubtree(L"Resources"),
        manager.CreateResourceContext());
}
bool MainWindow::CanRestoreStartupSourceAccess() const
{
    requireUiThread();
    return startupRecoveryError_.has_value() && CanRetryRecovery();
}
hstring MainWindow::StartupRecoveredTransactionCount() const
{
    requireUiThread();
    return jpg_spinner::presentation::formatUnsignedInteger(startupRecoveredTransactionCount_);
}
bool MainWindow::CanExportDiagnostics() const
{
    requireUiThread();
    return !closeAdmission_.closeRequested() && !startupRecoveryPending_ && !pickerActive_ && !operationEventActive_ &&
           diagnosticLog_;
}
bool MainWindow::HasDiagnosticLogError() const
{
    requireUiThread();
    return diagnosticLogError_;
}
Microsoft::UI::Xaml::Visibility MainWindow::regionVisibility(Presentation::ImageProcessingExperienceState state) const
{
    requireUiThread();
    const auto currentState = startupRecoveryError_ ? Presentation::ImageProcessingExperienceState::RecoveryRequired
                              : viewModel_          ? viewModel_.State()
                                                    : Presentation::ImageProcessingExperienceState::SourceSelection;
    return currentState == state ? Microsoft::UI::Xaml::Visibility::Visible
                                 : Microsoft::UI::Xaml::Visibility::Collapsed;
}
Microsoft::UI::Xaml::Visibility MainWindow::SourceSelectionRegionVisibility() const
{
    return regionVisibility(Presentation::ImageProcessingExperienceState::SourceSelection);
}
Microsoft::UI::Xaml::Visibility MainWindow::AnalysisRegionVisibility() const
{
    return regionVisibility(Presentation::ImageProcessingExperienceState::Analysis);
}
Microsoft::UI::Xaml::Visibility MainWindow::ReviewRegionVisibility() const
{
    return regionVisibility(Presentation::ImageProcessingExperienceState::Review);
}
Microsoft::UI::Xaml::Visibility MainWindow::ProcessingRegionVisibility() const
{
    return regionVisibility(Presentation::ImageProcessingExperienceState::Processing);
}
Microsoft::UI::Xaml::Visibility MainWindow::ResultsRegionVisibility() const
{
    return regionVisibility(Presentation::ImageProcessingExperienceState::Results);
}
Microsoft::UI::Xaml::Visibility MainWindow::RecoveryRequiredRegionVisibility() const
{
    return regionVisibility(Presentation::ImageProcessingExperienceState::RecoveryRequired);
}
Microsoft::UI::Xaml::Visibility MainWindow::ProcessingRowsRegionVisibility() const
{
    requireUiThread();
    return viewModel_ && (viewModel_.State() == Presentation::ImageProcessingExperienceState::Review ||
                          viewModel_.State() == Presentation::ImageProcessingExperienceState::Processing ||
                          viewModel_.State() == Presentation::ImageProcessingExperienceState::Results ||
                          viewModel_.State() == Presentation::ImageProcessingExperienceState::RecoveryRequired)
               ? Microsoft::UI::Xaml::Visibility::Visible
               : Microsoft::UI::Xaml::Visibility::Collapsed;
}
Microsoft::UI::Xaml::Visibility MainWindow::BackupDestinationVisibility() const
{
    requireUiThread();
    return viewModel_ &&
                   viewModel_.OutputDisposition() ==
                       Presentation::OutputDisposition::ReplaceOriginalWithVerifiedBackup &&
                   ProcessingRowsRegionVisibility() == Microsoft::UI::Xaml::Visibility::Visible
               ? Microsoft::UI::Xaml::Visibility::Visible
               : Microsoft::UI::Xaml::Visibility::Collapsed;
}
Microsoft::UI::Xaml::Visibility MainWindow::OperationProgressVisibility() const
{
    requireUiThread();
    return viewModel_ && (viewModel_.State() == Presentation::ImageProcessingExperienceState::Analysis ||
                          viewModel_.State() == Presentation::ImageProcessingExperienceState::Processing)
               ? Microsoft::UI::Xaml::Visibility::Visible
               : Microsoft::UI::Xaml::Visibility::Collapsed;
}
void MainWindow::setViewModel(Presentation::MainWindowViewModel const &viewModel)
{
    requireUiThread();
    viewModelSubscription_.revoke();
    viewModel_ = viewModel;
    if (viewModel_)
        viewModelSubscription_ =
            viewModel_.PropertyChanged(auto_revoke, [weak = get_weak()](auto const &, auto const &arguments) {
                if (auto window = weak.get())
                    if (arguments.PropertyName() == L"State" || arguments.PropertyName() == L"CanSelectSourceFolder" ||
                        arguments.PropertyName() == L"OutputDisposition")
                        window->notifyShellState();
            });
    synchronizeProcessingOptionControls();
    propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{L"ViewModel"});
    notifyShellState();
}
void MainWindow::notifyShellState()
{
    requireUiThread();
    for (const auto *property :
         {L"CanPickSourceFolder", L"IsWorkflowInteractionEnabled", L"CanRetryRecovery", L"IsStartupRecoveryPending",
          L"HasStartupRecoveryError", L"StartupRecoveryErrorMessage", L"CanRestoreStartupSourceAccess",
          L"StartupRecoveredTransactionCount", L"CanExportDiagnostics", L"HasDiagnosticLogError",
          L"SourceSelectionRegionVisibility", L"AnalysisRegionVisibility", L"ReviewRegionVisibility",
          L"ProcessingRegionVisibility", L"ResultsRegionVisibility", L"RecoveryRequiredRegionVisibility",
          L"ProcessingRowsRegionVisibility", L"BackupDestinationVisibility", L"OperationProgressVisibility"})
        propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{property});
    closeAfterWorkSettles();
}
bool MainWindow::hasActiveShellWork() const
{
    requireUiThread();
    return pickerActive_ || operationEventActive_ || (startupRecoveryStarted_ && startupRecoveryPending_);
}
void MainWindow::handleClosing(const Microsoft::UI::Windowing::AppWindowClosingEventArgs &arguments)
{
    requireUiThread();
    if (closeAdmission_.requestClose(hasActiveShellWork()))
        return; // Let Windows perform its ordinary idle-window close.
    arguments.Cancel(true);
    startupRecoveryCancellation_.request_stop();
    if (activeOperation_)
    {
        try
        {
            activeOperation_.Cancel();
        }
        catch (const hresult_error &)
        {
            // A native operation can finish during the close request. Do not
            // force termination or treat a refused cancel as settled work.
        }
    }
    // Cancel requests stop; it is NOT proof that the current coefficient call
    // or journaled commit has returned. The actual completion path closes later.
    notifyShellState();
}
void MainWindow::closeAfterWorkSettles()
{
    requireUiThread();
    const auto active = hasActiveShellWork();
    if (!active)
        activeOperation_ = nullptr;
    if (!deferredCloseDispatched_ && closeAdmission_.shouldCloseAfterWorkSettles(active))
    {
        deferredCloseDispatched_ = true;
        Close();
    }
}
Windows::Foundation::IAsyncAction MainWindow::recoverStartupAsync()
{
    auto lifetime = get_strong();
    const auto queue = DispatcherQueue();
    startupRecoveryCancellation_ = std::stop_source{};
    const auto cancellation = startupRecoveryCancellation_.get_token();
    const auto beganAt = std::chrono::steady_clock::now();
    startupRecoveryPending_ = true;
    notifyShellState();
    using Result =
        jpg_spinner::domain::ImageProcessingResult<jpg_spinner::storage::ImageFileTransactionRecoverySummary>;
    std::optional<Result> result;
    std::shared_ptr<jpg_spinner::diagnostics::LocalDiagnosticLog> log;
    bool logError{};
    try
    {
        // App-local storage is the only directory created at startup. Never
        // reconstruct a user's source grant from a journal's display path.
        const auto local = Windows::Storage::ApplicationData::Current().LocalFolder();
        const auto store = co_await local.CreateFolderAsync(L"ImageTransactionJournals",
                                                            Windows::Storage::CreationCollisionOption::OpenIfExists);
        co_await resume_background();
        {
            auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
            // Acquire the native access-list object in the apartment that
            // enumerates/restores its capabilities, without requiring an
            // instance acquired on the XAML UI thread to be agile.
            const auto cache = Windows::Storage::AccessCache::StorageApplicationPermissions::FutureAccessList();
            result.emplace(jpg_spinner::presentation::recoverStartupTransactions(store, cache, cancellation));
            try
            {
                const auto folder =
                    local
                        .CreateFolderAsync(L"LocalDiagnostics", Windows::Storage::CreationCollisionOption::OpenIfExists)
                        .get();
                log = std::make_shared<jpg_spinner::diagnostics::LocalDiagnosticLog>(folder);
                GUID identifier{};
                check_hresult(CoCreateGuid(&identifier));
                logError = !log->append(jpg_spinner::diagnostics::captureImageProcessingDiagnosticRecord(
                    std::bit_cast<guid>(identifier), jpg_spinner::domain::ImageProcessingStage::TransactionRecovery,
                    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - beganAt),
                    result->errorIfPresent()));
            }
            catch (...)
            {
                // Diagnostic storage must never convert a successful recovery
                // into a refusal. No native exception text enters the logger.
                logError = true;
            }
        }
    }
    catch (...)
    {
        // No native message/private path is reflected into UI or diagnostics.
        result.emplace(Result::failure({jpg_spinner::domain::ImageProcessingErrorCode::RecoveryConflict,
                                        jpg_spinner::domain::ImageProcessingStage::TransactionRecovery}));
    }
    co_await wil::resume_foreground(queue);
    diagnosticLog_ = std::move(log);
    diagnosticLogError_ = logError;
    startupRecoveryError_.reset();
    if (const auto error = result->errorIfPresent())
        startupRecoveryError_.emplace(*error);
    else
        startupRecoveredTransactionCount_ += result->valueIfPresent()->recoveredTransactions.size();
    startupRecoveryPending_ = false;
    notifyShellState();
}
void MainWindow::showIntegrationError()
{
    requireUiThread();
    if (closeAdmission_.closeRequested())
        return; // Expected cancellation during close is not an integration error.
    using namespace Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    const auto text = jpg_spinner::presentation::loadGenericImageProcessingErrorText(
        manager.MainResourceMap().GetSubtree(L"Resources"), manager.CreateResourceContext());
    // Native exception messages and full private paths never become UI prose.
    IntegrationErrorBar().Title(text.title);
    IntegrationErrorBar().Message(text.explanation + L"\n" + text.remedy);
    IntegrationErrorBar().IsOpen(true);
}
Windows::Foundation::IAsyncAction MainWindow::selectPickedSourceAsync(Windows::Storage::StorageFolder selectedFolder)
{
    auto lifetime = get_strong();
    const auto queue = DispatcherQueue();
    const auto applicationOwnedFolders = applicationOwnedFolders_; // Copy on the owning UI thread.
    const auto journalStore = co_await Windows::Storage::ApplicationData::Current().LocalFolder().CreateFolderAsync(
        L"ImageTransactionJournals", Windows::Storage::CreationCollisionOption::OpenIfExists);
    // Preserve the actual picker grant. Only app-local state is created here;
    // no parent/sibling is acquired from a display path or file-system lookup.
    co_await resume_background();
    std::optional<jpg_spinner::presentation::SelectedFolderBatchComposition> composition;
    {
        // Co/RoInitialize and Uninitialize must execute on the same worker;
        // never retain this apartment guard across the return to the UI queue.
        auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
        composition.emplace(jpg_spinner::presentation::composeSelectedFolderBatch(selectedFolder, journalStore,
                                                                                  applicationOwnedFolders));
    }
    co_await wil::resume_foreground(queue);
    const auto newIdentity = std::bit_cast<guid>(composition->request.identifier.fullIdentity());
    Presentation::implementation::MainWindowViewModel::OperationFailureDiagnosticObserver failureDiagnostics;
    if (const auto log = diagnosticLog_)
        failureDiagnostics = [log, newIdentity](const auto &failure, std::chrono::milliseconds elapsed) {
            // This callback executes on the model's owned MTA. No source,
            // display path, metadata value or native message enters the sink.
            static_cast<void>(log->append(jpg_spinner::diagnostics::captureImageProcessingDiagnosticRecord(
                newIdentity, failure.stage, elapsed, &failure)));
        };
    auto model = make_self<Presentation::implementation::MainWindowViewModel>(
        queue, coordinator_, composition->transactionEngine, std::move(failureDiagnostics));
    model->selectSource(std::move(composition->request), composition->sourceFolderDisplayName,
                        composition->backupRootDisplayPath);
    selectedSourceFolder_ = std::move(selectedFolder);
    batchIdentity_ = newIdentity;
    transactionEngine_ = composition->transactionEngine;
    transactionStartRequested_ = false;
    setViewModel(model.as<Presentation::MainWindowViewModel>());
}
fire_and_forget MainWindow::SourcePicker_Click(Windows::Foundation::IInspectable const &,
                                               Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (!CanPickSourceFolder())
        co_return;
    pickerActive_ = true;
    notifyShellState();
    try
    {
        Windows::Storage::Pickers::FolderPicker picker;
        picker.FileTypeFilter().Append(L"*");
        HWND windowHandle{};
        check_hresult(m_inner.as<::IWindowNative>()->get_WindowHandle(&windowHandle));
        // StorageFolder is the granted capability required by our Storage
        // adapter. The newer path-returning picker is not a substitute grant.
        check_hresult(picker.as<::IInitializeWithWindow>()->Initialize(windowHandle));
        const auto picking = picker.PickSingleFolderAsync();
        activeOperation_ = picking.as<Windows::Foundation::IAsyncInfo>();
        if (auto selectedFolder = co_await picking)
        {
            releaseReconciledSourceAccess();
            co_await selectPickedSourceAsync(std::move(selectedFolder));
            IntegrationErrorBar().IsOpen(false);
        }
        // Cancellation does not replace or invalidate the previous model/review.
    }
    catch (...)
    {
        showIntegrationError();
    }
    pickerActive_ = false;
    notifyShellState();
}
void MainWindow::synchronizeProcessingOptionControls()
{
    if (!viewModel_)
        return;
    updatingProcessingOptionControls_ = true;
    // Indices are local control positions, never native enum wire values.
    TraversalScopeControl().SelectedIndex(
        viewModel_.TraversalScope() == Presentation::TraversalScope::SelectedFolderOnly ? 0 : 1);
    OutputDispositionControl().SelectedIndex(
        viewModel_.OutputDisposition() == Presentation::OutputDisposition::CreateCorrectedCopy ? 0 : 1);
    EdgeHandlingControl().SelectedIndex(
        viewModel_.EdgeHandlingPolicy() == Presentation::EdgeHandlingPolicy::RequirePerfectCoefficientTransform ? 0
                                                                                                                : 1);
    switch (viewModel_.OutputScanOrganization())
    {
    case Presentation::OutputScanOrganization::PreserveSource:
        ScanOrganizationControl().SelectedIndex(0);
        break;
    case Presentation::OutputScanOrganization::SequentialDct:
        ScanOrganizationControl().SelectedIndex(1);
        break;
    case Presentation::OutputScanOrganization::ProgressiveDct:
        ScanOrganizationControl().SelectedIndex(2);
        break;
    default:
        throw hresult_invalid_argument();
    }
    updatingProcessingOptionControls_ = false;
}
fire_and_forget MainWindow::ProcessingOptions_SelectionChanged(
    Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &)
{
    auto lifetime = get_strong();
    // XAML initialization can raise this before every named control exists.
    if (closeAdmission_.closeRequested() || updatingProcessingOptionControls_ || pickerActive_ ||
        operationEventActive_ || !viewModel_ || !viewModel_.CanSelectSourceFolder())
        co_return;
    try
    {
        const int traversal = TraversalScopeControl().SelectedIndex();
        const int disposition = OutputDispositionControl().SelectedIndex();
        const int edge = EdgeHandlingControl().SelectedIndex();
        const int scans = ScanOrganizationControl().SelectedIndex();
        if (traversal < 0 || traversal > 1 || disposition < 0 || disposition > 1 || edge < 0 || edge > 1 || scans < 0 ||
            scans > 2)
            throw hresult_invalid_argument();
        if (viewModel_.State() == Presentation::ImageProcessingExperienceState::Results)
        {
            pickerActive_ = true;
            notifyShellState();
            releaseReconciledSourceAccess();
            // Results' request belongs to an executed UUID. Compose a fresh
            // native batch before applying the user's newly selected choices.
            co_await selectPickedSourceAsync(selectedSourceFolder_);
        }
        using ScanOrganization = Presentation::OutputScanOrganization;
        viewModel_.SetProcessingOptions(
            disposition == 0 ? Presentation::OutputDisposition::CreateCorrectedCopy
                             : Presentation::OutputDisposition::ReplaceOriginalWithVerifiedBackup,
            traversal == 0 ? Presentation::TraversalScope::SelectedFolderOnly
                           : Presentation::TraversalScope::SelectedFolderAndDescendants,
            edge == 0 ? Presentation::EdgeHandlingPolicy::RequirePerfectCoefficientTransform
                      : Presentation::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits,
            scans == 0 ? ScanOrganization::PreserveSource
                       : (scans == 1 ? ScanOrganization::SequentialDct : ScanOrganization::ProgressiveDct));
        synchronizeProcessingOptionControls();
    }
    catch (...)
    {
        synchronizeProcessingOptionControls();
        showIntegrationError();
    }
    pickerActive_ = false;
    notifyShellState();
}
fire_and_forget MainWindow::Analyze_Click(Windows::Foundation::IInspectable const &,
                                          Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (closeAdmission_.closeRequested() || pickerActive_ || operationEventActive_ || !viewModel_ ||
        !viewModel_.CanAnalyzeSelectedSource())
        co_return;
    operationEventActive_ = true;
    notifyShellState();
    try
    {
        const auto analyzing = viewModel_.AnalyzeAsync();
        activeOperation_ = analyzing.as<Windows::Foundation::IAsyncInfo>();
        co_await analyzing;
    }
    catch (...)
    {
        showIntegrationError();
    }
    operationEventActive_ = false;
    notifyShellState();
}
void MainWindow::retainSelectedSourceForTransaction()
{
    if (!sourceAccessRetention_)
        sourceAccessRetention_.emplace(
            Windows::Storage::AccessCache::StorageApplicationPermissions::FutureAccessList());
    sourceAccessRetention_->retainSourceFolder(selectedSourceFolder_, batchIdentity_);
}
void MainWindow::releaseReconciledSourceAccess()
{
    if (!sourceAccessRetention_ || !sourceAccessRetention_->hasRetainedSource())
        return;
    if (!viewModel_ ||
        (transactionStartRequested_ && viewModel_.State() != Presentation::ImageProcessingExperienceState::Results &&
         viewModel_.State() != Presentation::ImageProcessingExperienceState::SourceSelection))
        throw hresult_illegal_method_call();
    // Results requires a trustworthy terminal summary without uncertain
    // transactions; SourceSelection after recovery requires successful native
    // reconciliation. Neither removal deletes originals, backups or journals.
    sourceAccessRetention_->releaseReconciledSource();
}
void MainWindow::rememberCompletedBatchFolders()
{
    // Call only after the awaited operation publishes its terminal/reconciled
    // UI state. The engine snapshot performs no file I/O. Original IDs travel
    // with actual grants; later discovery revalidates and pins them itself.
    const auto folders = transactionEngine_->applicationOwnedFolders();
    applicationOwnedFolders_.insert(applicationOwnedFolders_.end(), folders.begin(), folders.end());
}
fire_and_forget MainWindow::StartProcessing_Click(Windows::Foundation::IInspectable const &,
                                                  Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (closeAdmission_.closeRequested() || pickerActive_ || operationEventActive_ || !viewModel_ ||
        !viewModel_.CanBeginProcessing())
        co_return;
    operationEventActive_ = true;
    notifyShellState();
    try
    {
        // Retain access before the first transaction can publish a journal.
        retainSelectedSourceForTransaction();
        transactionStartRequested_ = true;
        const auto processing = viewModel_.BeginProcessingAsync();
        activeOperation_ = processing.as<Windows::Foundation::IAsyncInfo>();
        co_await processing;
        if (viewModel_.State() == Presentation::ImageProcessingExperienceState::Results)
        {
            rememberCompletedBatchFolders();
            releaseReconciledSourceAccess();
        }
    }
    catch (...)
    {
        showIntegrationError();
    }
    operationEventActive_ = false;
    notifyShellState();
}
fire_and_forget MainWindow::RetryRecovery_Click(Windows::Foundation::IInspectable const &,
                                                Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (CanRetryRecovery() && startupRecoveryError_)
    {
        const auto recovery = recoverStartupAsync();
        activeOperation_ = recovery.as<Windows::Foundation::IAsyncInfo>();
        co_await recovery;
        co_return;
    }
    if (closeAdmission_.closeRequested() || pickerActive_ || operationEventActive_ || !viewModel_ ||
        viewModel_.State() != Presentation::ImageProcessingExperienceState::RecoveryRequired)
        co_return;
    operationEventActive_ = true;
    notifyShellState();
    try
    {
        const auto recovery = viewModel_.RecoverAsync();
        activeOperation_ = recovery.as<Windows::Foundation::IAsyncInfo>();
        co_await recovery;
        if (viewModel_.State() == Presentation::ImageProcessingExperienceState::SourceSelection)
        {
            rememberCompletedBatchFolders();
            releaseReconciledSourceAccess();
            pickerActive_ = true;
            // Recovery closed the old batch. Do not expose its consumed UUID
            // for another analysis if fresh composition subsequently refuses.
            setViewModel(nullptr);
            co_await selectPickedSourceAsync(selectedSourceFolder_);
        }
    }
    catch (...)
    {
        showIntegrationError();
    }
    operationEventActive_ = false;
    pickerActive_ = false;
    notifyShellState();
}
fire_and_forget MainWindow::RestoreRecoverySource_Click(Windows::Foundation::IInspectable const &,
                                                        Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (!CanRestoreStartupSourceAccess())
        co_return;
    pickerActive_ = true;
    notifyShellState();
    try
    {
        Windows::Storage::Pickers::FolderPicker picker;
        picker.FileTypeFilter().Append(L"*");
        HWND handle{};
        check_hresult(m_inner.as<::IWindowNative>()->get_WindowHandle(&handle));
        check_hresult(picker.as<::IInitializeWithWindow>()->Initialize(handle));
        const auto picking = picker.PickSingleFolderAsync();
        activeOperation_ = picking.as<Windows::Foundation::IAsyncInfo>();
        if (const auto folder = co_await picking)
        {
            const auto cache = Windows::Storage::AccessCache::StorageApplicationPermissions::FutureAccessList();
            if (cache.Entries().Size() >= cache.MaximumItemsAllowed())
                throw hresult_error{E_OUTOFMEMORY};
            // This is a newly granted recovery capability, not permission for
            // a normal batch. Identity matching still refuses the wrong folder.
            // Preserve its token for future startup if reconciliation refuses;
            // never evict another entry or invent batch/transaction correlation.
            cache.Add(folder, L"Startup recovery source grant");
            const auto recovery = recoverStartupAsync();
            activeOperation_ = recovery.as<Windows::Foundation::IAsyncInfo>();
            co_await recovery;
        }
    }
    catch (...)
    {
        showIntegrationError();
    }
    pickerActive_ = false;
    notifyShellState();
}
fire_and_forget MainWindow::ExportDiagnostics_Click(Windows::Foundation::IInspectable const &sender,
                                                    Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (!CanExportDiagnostics())
        co_return;
    const auto queue = DispatcherQueue();
    const auto log = diagnosticLog_; // Capture the owned logger on the UI thread.
    // Disabling the invoking control moves WinUI focus away from it. Capture
    // its input modality before publishing the busy state; re-enabling alone
    // does not restore keyboard/screen-reader continuity after the native picker.
    const auto exportInitiator = sender.try_as<Microsoft::UI::Xaml::Controls::Control>();
    const auto initiatingFocusState =
        exportInitiator ? exportInitiator.FocusState() : Microsoft::UI::Xaml::FocusState::Unfocused;
    bool exportFailed{};
    pickerActive_ = true;
    notifyShellState();
    try
    {
        Windows::Storage::Pickers::FileSavePicker picker;
        using namespace Microsoft::Windows::ApplicationModel::Resources;
        ResourceManager resources{ResourceLoader::GetDefaultResourceFilePath()};
        const auto label = resources.MainResourceMap()
                               .GetSubtree(L"Resources")
                               .GetValue(L"Shell_DiagnosticFileType", resources.CreateResourceContext())
                               .ValueAsString();
        picker.FileTypeChoices().Insert(label, single_threaded_vector<hstring>({L".jsonl"}));
        picker.DefaultFileExtension(L".jsonl");
        picker.SuggestedFileName(L"JPG Spinner diagnostics");
        HWND handle{};
        check_hresult(m_inner.as<::IWindowNative>()->get_WindowHandle(&handle));
        check_hresult(picker.as<::IInitializeWithWindow>()->Initialize(handle));
        const auto picking = picker.PickSaveFileAsync();
        activeOperation_ = picking.as<Windows::Foundation::IAsyncInfo>();
        if (const auto destination = co_await picking)
        {
            // No logger snapshot or retention effect occurs if the picker was
            // cancelled. Export writes only sanitized bytes to this explicit
            // grant; no path is used to acquire another destination or source.
            if (!closeAdmission_.closeRequested())
            {
                const auto exporting = jpg_spinner::diagnostics::exportDiagnosticLogAsync(log, destination);
                activeOperation_ = exporting.as<Windows::Foundation::IAsyncInfo>();
                co_await exporting;
                diagnosticLogError_ = false;
            }
        }
    }
    catch (...)
    {
        exportFailed = true;
    }
    // Snapshot failures originate on the worker. Restore UI affinity before
    // status publication; no await is placed inside an exception handler.
    co_await wil::resume_foreground(queue);
    if (exportFailed)
        diagnosticLogError_ = true;
    pickerActive_ = false;
    notifyShellState();
    // Focus only the still-loaded, enabled initiator of this interaction, and
    // never pass Unfocused to Focus (the WinUI contract forbids it). Native
    // Focus may set only logical focus when another app owns the foreground;
    // deliberately do not activate a window or steal global desktop focus.
    if (!closeAdmission_.closeRequested() && exportInitiator && exportInitiator.IsLoaded() &&
        exportInitiator.IsEnabled() && initiatingFocusState != Microsoft::UI::Xaml::FocusState::Unfocused)
        (void)exportInitiator.Focus(initiatingFocusState);
}
fire_and_forget MainWindow::ReturnToSourceSelection_Click(Windows::Foundation::IInspectable const &,
                                                          Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (closeAdmission_.closeRequested() || pickerActive_ || operationEventActive_ || !viewModel_ ||
        !viewModel_.CanReturnToSourceSelection())
        co_return;
    const bool completedBatch = viewModel_.State() == Presentation::ImageProcessingExperienceState::Results;
    pickerActive_ = true;
    notifyShellState();
    try
    {
        releaseReconciledSourceAccess();
        // A completed batch's UUID/output tree must never be reused. A review
        // may retain its unexecuted identity, but invalidates its reviewed plan.
        if (completedBatch)
            co_await selectPickedSourceAsync(selectedSourceFolder_);
        else
            viewModel_.ReturnToSourceSelection();
    }
    catch (...)
    {
        showIntegrationError();
    }
    pickerActive_ = false;
    notifyShellState();
}
void MainWindow::CancelOperation_Click(Windows::Foundation::IInspectable const &,
                                       Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    if (!pickerActive_ && viewModel_ && viewModel_.CanCancelCurrentOperation())
        viewModel_.CancelCurrentOperation();
}
void MainWindow::TrimConsent_Click(Windows::Foundation::IInspectable const &sender,
                                   Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    if (closeAdmission_.closeRequested() || pickerActive_ || operationEventActive_ || !viewModel_ ||
        viewModel_.State() != Presentation::ImageProcessingExperienceState::Review)
        return;
    try
    {
        const auto row = sender.as<Microsoft::UI::Xaml::FrameworkElement>()
                             .DataContext()
                             .as<Presentation::ImageProcessingRowViewModel>();
        // Virtualized/recycled controls cannot approve another review's ordinal.
        std::uint32_t index{};
        if (!viewModel_.Rows().IndexOf(row, index))
            throw hresult_illegal_method_call();
        viewModel_.AcknowledgeEdgeTrimming(index);
    }
    catch (...)
    {
        showIntegrationError();
    }
}
event_token MainWindow::PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler)
{
    requireUiThread();
    return propertyChanged_.add(handler);
}
void MainWindow::PropertyChanged(event_token const &token)
{
    requireUiThread();
    propertyChanged_.remove(token);
}
} // namespace winrt::JpgSpinner::implementation
