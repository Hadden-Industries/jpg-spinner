#include "pch.h"

#include "MainWindow.xaml.h"
#include "ViewModels/MainWindowViewModel.h"
#include "Composition/SelectedFolderBatchComposition.h"
#include "Localization/ImageProcessingErrorText.h"
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.AccessCache.h>
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
    WorkflowContent().Loaded([weak = get_weak()](auto const &, auto const &) {
        if (auto window = weak.get())
        {
            window->updatePreferredMinimumWindowSize();
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
    return !pickerActive_ && !operationEventActive_ && (!viewModel_ || viewModel_.CanSelectSourceFolder());
}
bool MainWindow::IsWorkflowInteractionEnabled() const
{
    requireUiThread();
    return !pickerActive_;
}
bool MainWindow::CanRetryRecovery() const
{
    requireUiThread();
    return !pickerActive_ && !operationEventActive_ && viewModel_ &&
           viewModel_.State() == Presentation::ImageProcessingExperienceState::RecoveryRequired;
}
Microsoft::UI::Xaml::Visibility MainWindow::regionVisibility(Presentation::ImageProcessingExperienceState state) const
{
    requireUiThread();
    const auto currentState =
        viewModel_ ? viewModel_.State() : Presentation::ImageProcessingExperienceState::SourceSelection;
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
         {L"CanPickSourceFolder", L"IsWorkflowInteractionEnabled", L"CanRetryRecovery",
          L"SourceSelectionRegionVisibility", L"AnalysisRegionVisibility", L"ReviewRegionVisibility",
          L"ProcessingRegionVisibility", L"ResultsRegionVisibility", L"RecoveryRequiredRegionVisibility",
          L"ProcessingRowsRegionVisibility", L"BackupDestinationVisibility", L"OperationProgressVisibility"})
        propertyChanged_(*this, Microsoft::UI::Xaml::Data::PropertyChangedEventArgs{property});
}
void MainWindow::showIntegrationError()
{
    requireUiThread();
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
    auto model = make_self<Presentation::implementation::MainWindowViewModel>(queue, coordinator_,
                                                                              composition->transactionEngine);
    const auto newIdentity = std::bit_cast<guid>(composition->request.identifier.fullIdentity());
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
        if (auto selectedFolder = co_await picker.PickSingleFolderAsync())
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
    if (updatingProcessingOptionControls_ || pickerActive_ || operationEventActive_ || !viewModel_ ||
        !viewModel_.CanSelectSourceFolder())
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
    if (pickerActive_ || operationEventActive_ || !viewModel_ || !viewModel_.CanAnalyzeSelectedSource())
        co_return;
    operationEventActive_ = true;
    notifyShellState();
    try
    {
        co_await viewModel_.AnalyzeAsync();
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
    if (pickerActive_ || operationEventActive_ || !viewModel_ || !viewModel_.CanBeginProcessing())
        co_return;
    operationEventActive_ = true;
    notifyShellState();
    try
    {
        // Retain access before the first transaction can publish a journal.
        retainSelectedSourceForTransaction();
        transactionStartRequested_ = true;
        co_await viewModel_.BeginProcessingAsync();
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
    if (pickerActive_ || operationEventActive_ || !viewModel_ ||
        viewModel_.State() != Presentation::ImageProcessingExperienceState::RecoveryRequired)
        co_return;
    operationEventActive_ = true;
    notifyShellState();
    try
    {
        co_await viewModel_.RecoverAsync();
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
fire_and_forget MainWindow::ReturnToSourceSelection_Click(Windows::Foundation::IInspectable const &,
                                                          Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    if (pickerActive_ || operationEventActive_ || !viewModel_ || !viewModel_.CanReturnToSourceSelection())
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
    if (pickerActive_ || operationEventActive_ || !viewModel_ ||
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
