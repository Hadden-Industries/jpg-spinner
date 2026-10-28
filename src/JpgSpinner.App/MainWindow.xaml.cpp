#include "pch.h"

#include "MainWindow.xaml.h"
#include "ShellContentPage.xaml.h"
#include "ViewModels/MainWindowViewModel.h"
#include "Composition/SelectedFolderBatchComposition.h"
#include "Composition/StartupRecoveryComposition.h"
#include "Localization/ImageProcessingErrorText.h"
#include "Localization/UnsignedIntegerText.h"
#include "Diagnostics/DiagnosticLogExport.h"
#include "Imaging/ProgressiveJpegExample.h"
#include <jpg_spinner/jpeg/LibJpegTurboTransformationEngine.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Storage.Pickers.h>
#include <winrt/Windows.Storage.AccessCache.h>
#include <winrt/Windows.Storage.Provider.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>
#include <winrt/Microsoft.UI.Xaml.Navigation.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.System.h>
#include <cstring>
#include <limits>
#include <algorithm>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <microsoft.ui.xaml.window.h>
#include <shobjidl_core.h>
#include <roapi.h>
#include <wil/cppwinrt_helpers.h>
#include <wil/resource.h>
#include <bit>
#include <cmath>
#include <optional>
#include <string_view>

// MIDL emits the activation-factory entry point in this generated source.
// Include it in the owning runtime class translation unit, as prescribed by
// the C++/WinRT project template, so module.g.cpp can resolve the factory.
#if __has_include("MainWindow.g.cpp")
#include "MainWindow.g.cpp"
#endif

namespace winrt::JpgSpinner::implementation
{
namespace
{
// Version-1 persisted selections use this explicit order and range contract.
// Keep validation and native composite keys shared by loading, editing/saving
// and conversion to domain-facing presentation options.
constexpr std::array maximumProcessingOptionSelectionIndices{1, 2, 1, 2};
constexpr std::array<std::wstring_view, 4> processingPreferenceSelectionKeys{
    L"TraversalScopeSelection", L"OutputDispositionSelection", L"EdgeHandlingSelection", L"ScanOrganizationSelection"};
constexpr std::wstring_view parallelImagePreferenceKey = L"MaximumConcurrentImageCount";
constexpr std::wstring_view automaticMemoryPreferenceKey = L"AutomaticProcessingMemoryBudget";
constexpr std::wstring_view customMemoryPreferenceKey = L"ProcessingMemoryBudgetMebibytes";
bool areValidProcessingOptionSelections(const std::array<int, 4> &selections)
{
    for (std::size_t index = 0; index < selections.size(); ++index)
        if (selections[index] < 0 || selections[index] > maximumProcessingOptionSelectionIndices[index])
            return false;
    return true;
}
bool isPositiveWholeNumberWithin(const double value, const double maximum)
{
    return std::isfinite(value) && value >= 1 && value <= maximum && std::floor(value) == value;
}
} // namespace
MainWindow::MainWindow()
{
    InitializeComponent();
    using jpg_spinner::batch_processing::BatchExecutionLimits;
    MaximumConcurrentImagesControl().Maximum(BatchExecutionLimits::deviceConcurrentImageLimit());
    MaximumConcurrentImagesControl().Value(BatchExecutionLimits::recommended().maximumConcurrentImageCount);
    // This is the range of an exactly representable whole-MiB preference, not a
    // claim that the app can allocate it. Live system headroom always wins.
    ProcessingMemoryBudgetControl().Maximum((std::numeric_limits<std::uint32_t>::max)());
    loadProcessingPreferences();
    processingPreferencesReady_ = true;
    // Compose each view once, then let Frame own page history. Basic string
    // parameters follow Microsoft's recommendation and cannot retain another
    // window/model through the journal. Returning to a dirty selected folder
    // resumes read-only analysis, never image transformation or saving.
    // https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/microsoft.ui.xaml.controls.frame.navigate
    ShellViews().Children().Clear();
    ShellFrame().Navigated([weak = get_weak()](const Windows::Foundation::IInspectable &,
                                               const Microsoft::UI::Xaml::Navigation::NavigationEventArgs &arguments) {
        if (auto window = weak.get())
        {
            const auto destination = unbox_value<hstring>(arguments.Parameter());
            Microsoft::UI::Xaml::UIElement content{nullptr};
            if (destination == L"Workflow")
                content = window->WorkflowView();
            else if (destination == L"Settings")
                content = window->SettingsView();
            else if (destination == L"Crop")
                content = window->CropExplanationView();
            else if (destination == L"Progressive")
                content = window->ProgressiveExplanationView();
            else if (destination == L"Privacy")
                content = window->PrivacyView();
            else
                throw hresult_invalid_argument();
            const auto page = window->ShellFrame().Content().as<Microsoft::UI::Xaml::Controls::Page>();
            page.Tag(arguments.Parameter());
            page.Content(content);
            if (destination == L"Workflow")
                window->analyzeOnWorkflowEntryAsync();
        }
    });
    // VirtualKey defines these browser/navigation keys, but WinUI's XAML
    // converter does not accept their names. Use the documented native setter
    // rather than numeric literals or a second window-message input path.
    // https://learn.microsoft.com/windows/apps/develop/input/keyboard-accelerators
    using Windows::System::VirtualKey;
    for (const auto key : {VirtualKey::GoBack, VirtualKey::GoForward})
    {
        Microsoft::UI::Xaml::Input::KeyboardAccelerator accelerator;
        accelerator.Key(key);
        accelerator.Invoked([weak = get_weak(), key](auto const &, auto const &arguments) {
            if (auto window = weak.get())
                arguments.Handled(key == VirtualKey::GoBack ? window->tryNavigateBack() : window->tryNavigateForward());
        });
        ShellRoot().KeyboardAccelerators().Append(accelerator);
    }
    ShellRoot().AddHandler(
        Microsoft::UI::Xaml::UIElement::PointerPressedEvent(),
        box_value(Microsoft::UI::Xaml::Input::PointerEventHandler{
            [weak = get_weak()](auto const &, const Microsoft::UI::Xaml::Input::PointerRoutedEventArgs &arguments) {
                if (auto window = weak.get())
                {
                    const auto properties = arguments.GetCurrentPoint(window->ShellRoot()).Properties();
                    // A discrete X-button press navigates once. Holding it while
                    // pressing another mouse button must not navigate again.
                    using Microsoft::UI::Input::PointerUpdateKind;
                    if ((properties.IsXButton1Pressed() &&
                         properties.PointerUpdateKind() == PointerUpdateKind::XButton1Pressed &&
                         window->tryNavigateBack()) ||
                        (properties.IsXButton2Pressed() &&
                         properties.PointerUpdateKind() == PointerUpdateKind::XButton2Pressed &&
                         window->tryNavigateForward()))
                        arguments.Handled(true);
                }
            }}),
        true); // Child controls may handle PointerPressed first.
    navigateToShellPage(L"Workflow");
    // The native TitleBar owns caption/drag regions, RTL and material fallback.
    ExtendsContentIntoTitleBar(true);
    SetTitleBar(AppTitleBar());
    closingEventSource_ = AppWindow();
    closingEventToken_ = closingEventSource_.Closing([weak = get_weak()](auto const &, auto const &arguments) {
        if (auto window = weak.get())
            window->handleClosing(arguments);
    });
    ShellRoot().Loaded([weak = get_weak()](auto const &, auto const &) {
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
                window->ShellRoot().XamlRoot().Changed(auto_revoke, [weak](auto const &, auto const &) {
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
    const auto scale = ShellRoot().XamlRoot().RasterizationScale();
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
bool MainWindow::CanEditProcessingSettings() const
{
    // Settings are available before selecting a folder. During analysis,
    // processing and recovery the existing admission gate locks their editing.
    return CanPickSourceFolder();
}
Microsoft::UI::Xaml::Visibility MainWindow::LandingRegionVisibility() const
{
    requireUiThread();
    return !viewModel_ && !startupRecoveryError_ ? Microsoft::UI::Xaml::Visibility::Visible
                                                 : Microsoft::UI::Xaml::Visibility::Collapsed;
}
Microsoft::UI::Xaml::Visibility MainWindow::WorkflowRegionVisibility() const
{
    requireUiThread();
    return viewModel_ || startupRecoveryError_ ? Microsoft::UI::Xaml::Visibility::Visible
                                               : Microsoft::UI::Xaml::Visibility::Collapsed;
}
Microsoft::UI::Xaml::Visibility MainWindow::StartupRecoverySummaryVisibility() const
{
    requireUiThread();
    return startupRecoveredTransactionCount_ != 0 ? Microsoft::UI::Xaml::Visibility::Visible
                                                  : Microsoft::UI::Xaml::Visibility::Collapsed;
}
void MainWindow::Settings_Click(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    navigateToShellPage(L"Settings");
}
void MainWindow::CloseSettings_Click(Windows::Foundation::IInspectable const &,
                                     Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    tryNavigateBack();
}
void MainWindow::navigateToShellPage(const hstring &destination)
{
    requireUiThread();
    const auto current = ShellFrame().Content().try_as<Microsoft::UI::Xaml::Controls::Page>();
    if (current && current.Tag() && unbox_value<hstring>(current.Tag()) == destination)
        return; // Reopening Settings must not add duplicate history entries.
    ShellFrame().Navigate(xaml_typename<JpgSpinner::ShellContentPage>(), box_value(destination));
}
bool MainWindow::tryNavigateBack()
{
    requireUiThread();
    if (closeAdmission_.closeRequested() || pickerActive_ || !ShellFrame().CanGoBack())
        return false;
    ShellFrame().GoBack();
    return true;
}
bool MainWindow::tryNavigateForward()
{
    requireUiThread();
    if (closeAdmission_.closeRequested() || pickerActive_ || !ShellFrame().CanGoForward())
        return false;
    ShellFrame().GoForward();
    return true;
}
void MainWindow::NavigateBack_Invoked(const Microsoft::UI::Xaml::Input::KeyboardAccelerator &,
                                      const Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs &arguments)
{
    arguments.Handled(tryNavigateBack());
}
void MainWindow::NavigateForward_Invoked(
    const Microsoft::UI::Xaml::Input::KeyboardAccelerator &,
    const Microsoft::UI::Xaml::Input::KeyboardAcceleratorInvokedEventArgs &arguments)
{
    arguments.Handled(tryNavigateForward());
}
void MainWindow::Logo_Click(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    // Keep the original two-second Easter egg as an explicit user action.
    // Windows' reduced-motion preference also applies to decorative animation.
    LogoSpin().Stop();
    if (Windows::UI::ViewManagement::UISettings{}.AnimationsEnabled())
        LogoSpin().Begin();
}
void MainWindow::showContactFeedback(const wchar_t *resourceKey,
                                     Microsoft::UI::Xaml::Controls::InfoBarSeverity severity)
{
    using namespace Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    PrivacyContactFeedback().Message(manager.MainResourceMap()
                                         .GetSubtree(L"Resources")
                                         .GetValue(resourceKey, manager.CreateResourceContext())
                                         .ValueAsString());
    PrivacyContactFeedback().Severity(severity);
    PrivacyContactFeedback().IsOpen(true);
}
fire_and_forget MainWindow::PrivacyEmail_Click(Windows::Foundation::IInspectable const &sender,
                                               Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    const Windows::Foundation::Uri uri{
        unbox_value<hstring>(sender.as<Microsoft::UI::Xaml::Controls::HyperlinkButton>().Tag())};
    PrivacyContactFeedback().IsOpen(false);
    try
    {
        // Query detects available handlers, not a configured default. Windows
        // may still show its chooser; keep Copy email address independently
        // available even when chooser activation itself reports success. Also
        // check launch completion: associations can change after the query.
        const auto support =
            co_await Windows::System::Launcher::QueryUriSupportAsync(uri, Windows::System::LaunchQuerySupportType::Uri);
        if (support == Windows::System::LaunchQuerySupportStatus::Available &&
            co_await Windows::System::Launcher::LaunchUriAsync(uri))
            co_return;
    }
    catch (const hresult_error &)
    {
        // A refused or failed activation has the same useful local fallback;
        // native exception details do not become privacy-page prose.
    }
    showContactFeedback(L"Shell_EmailUnavailable", Microsoft::UI::Xaml::Controls::InfoBarSeverity::Informational);
}
void MainWindow::CopyContactEmail_Click(Windows::Foundation::IInspectable const &,
                                        Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    try
    {
        using namespace Windows::ApplicationModel::DataTransfer;
        // The contact URI is the single source for launch, copy and publication.
        const Windows::Foundation::Uri uri{unbox_value<hstring>(PrivacyEmailLink().Tag())};
        DataPackage contact;
        contact.SetText(uri.Path());
        Clipboard::SetContent(contact);
        showContactFeedback(L"Shell_EmailCopied", Microsoft::UI::Xaml::Controls::InfoBarSeverity::Success);
    }
    catch (const hresult_error &)
    {
        showContactFeedback(L"Shell_EmailCopyFailed", Microsoft::UI::Xaml::Controls::InfoBarSeverity::Warning);
    }
}
fire_and_forget MainWindow::SettingsHelp_Click(Windows::Foundation::IInspectable const &sender,
                                               Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    const auto topic = unbox_value<hstring>(sender.as<Microsoft::UI::Xaml::Controls::HyperlinkButton>().Tag());
    navigateToShellPage(topic);
    if (topic == L"Progressive" && !progressiveExamplesLoaded_)
    {
        ProgressiveExampleProgress().IsActive(true);
        ProgressiveExampleError().IsOpen(false);
        try
        {
            co_await loadProgressiveExamplesAsync();
            progressiveExamplesLoaded_ = true;
        }
        catch (...)
        {
            ProgressiveExampleError().IsOpen(true);
        }
        ProgressiveExampleProgress().IsActive(false);
    }
}
Windows::Foundation::IAsyncAction MainWindow::loadProgressiveExamplesAsync()
{
    auto lifetime = get_strong();
    const auto queue = DispatcherQueue();
    const auto file = co_await Windows::Storage::StorageFile::GetFileFromApplicationUriAsync(
        Windows::Foundation::Uri{L"ms-appx:///Assets/Help/ProgressiveEncodingExample.jpg"});
    const std::filesystem::path path{file.Path().c_str()};
    co_await resume_background();
    std::array<jpg_spinner::presentation::ProgressiveJpegExampleFrame, 3> frames;
    {
        auto apartment = wil::RoInitialize(RO_INIT_MULTITHREADED);
        frames = jpg_spinner::presentation::decodeProgressiveJpegExample(path);
    }
    co_await wil::resume_foreground(queue);
    const std::array images{ProgressiveFirstImage(), ProgressiveMiddleImage(), ProgressiveFinalImage()};
    for (std::size_t index = 0; index < frames.size(); ++index)
    {
        const auto &frame = frames[index];
        Microsoft::UI::Xaml::Media::Imaging::WriteableBitmap bitmap{static_cast<int32_t>(frame.widthInPixels),
                                                                    static_cast<int32_t>(frame.heightInPixels)};
        const auto buffer = bitmap.PixelBuffer();
        if (buffer.Length() != frame.premultipliedBgraPixels.size())
            throw hresult_invalid_argument();
        std::memcpy(buffer.data(), frame.premultipliedBgraPixels.data(), frame.premultipliedBgraPixels.size());
        bitmap.Invalidate();
        images[index].Source(bitmap);
    }
}
void MainWindow::CloseSettingsHelp_Click(Windows::Foundation::IInspectable const &,
                                         Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    tryNavigateBack();
}
hstring MainWindow::ProcessingSettingsSummary()
{
    requireUiThread();
    std::wstring summary;
    for (const auto &control :
         {TraversalScopeControl(), OutputDispositionControl(), EdgeHandlingControl(), ScanOrganizationControl()})
    {
        if (const auto selected = control.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::RadioButton>())
        {
            if (!summary.empty())
                summary += L" · ";
            if (control == OutputDispositionControl())
                summary += OutputDispositionDescription();
            else
                summary += unbox_value<hstring>(selected.Content());
        }
    }
    using namespace Microsoft::Windows::ApplicationModel::Resources;
    ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
    summary += L" · ";
    summary += manager.MainResourceMap()
                   .GetSubtree(L"Resources")
                   .GetValue(L"Shell_ParallelPhotosSummary", manager.CreateResourceContext())
                   .ValueAsString();
    summary += MaximumConcurrentImageCountText();
    summary += L" · ";
    const auto memoryBudget = selectedProcessingMemoryBudgetBytes();
    summary += manager.MainResourceMap()
                   .GetSubtree(L"Resources")
                   .GetValue(memoryBudget ? L"Shell_MemoryBudgetCustomSummary" : L"Shell_MemoryBudgetAutomaticSummary",
                             manager.CreateResourceContext())
                   .ValueAsString();
    if (memoryBudget)
    {
        summary += jpg_spinner::presentation::formatUnsignedInteger(
            *memoryBudget / jpg_spinner::batch_processing::BatchExecutionLimits::bytesPerMebibyte);
        summary += L" MiB"; // IEC unit symbol is language-independent.
    }
    return hstring{summary};
}
hstring MainWindow::OutputDispositionDescription()
{
    const auto control = OutputDispositionControl();
    if (!control)
        return {};
    if (control.SelectedIndex() == 2)
    {
        using namespace Microsoft::Windows::ApplicationModel::Resources;
        ResourceManager manager{ResourceLoader::GetDefaultResourceFilePath()};
        return manager.MainResourceMap()
            .GetSubtree(L"Resources")
            .GetValue(L"Shell_OutputSummaryNoBackup", manager.CreateResourceContext())
            .ValueAsString();
    }
    const auto selected = control.SelectedItem().try_as<Microsoft::UI::Xaml::Controls::RadioButton>();
    return selected ? unbox_value<hstring>(selected.Content()) : hstring{};
}
bool MainWindow::HasNoPlannedCorrections() const
{
    return viewModel_ && viewModel_.State() == Presentation::ImageProcessingExperienceState::Review &&
           viewModel_.EligibleFileCount() == 0;
}
Microsoft::UI::Xaml::Visibility MainWindow::StartupRecoveryProgressVisibility() const
{
    return IsStartupRecoveryPending() ? Microsoft::UI::Xaml::Visibility::Visible
                                      : Microsoft::UI::Xaml::Visibility::Collapsed;
}
std::uint32_t MainWindow::selectedMaximumConcurrentImageCount()
{
    const auto control = MaximumConcurrentImagesControl();
    if (!control)
        return 1; // Initial compiled bindings may precede the native control.
    const auto value = control.Value();
    if (!isPositiveWholeNumberWithin(value, control.Maximum()))
        throw hresult_invalid_argument();
    return static_cast<std::uint32_t>(value);
}
hstring MainWindow::MaximumConcurrentImageCountText()
{
    return jpg_spinner::presentation::formatUnsignedInteger(selectedMaximumConcurrentImageCount());
}
Microsoft::UI::Xaml::Visibility MainWindow::CustomMemoryBudgetVisibility()
{
    const auto automatic = AutomaticMemoryBudgetControl();
    return automatic && !automatic.IsOn() ? Microsoft::UI::Xaml::Visibility::Visible
                                          : Microsoft::UI::Xaml::Visibility::Collapsed;
}
std::optional<std::uint64_t> MainWindow::selectedProcessingMemoryBudgetBytes()
{
    const auto automatic = AutomaticMemoryBudgetControl();
    if (!automatic || automatic.IsOn())
        return std::nullopt;
    const auto control = ProcessingMemoryBudgetControl();
    const auto value = control.Value();
    if (!isPositiveWholeNumberWithin(value, control.Maximum()))
        throw hresult_invalid_argument();
    return static_cast<std::uint64_t>(value) * jpg_spinner::batch_processing::BatchExecutionLimits::bytesPerMebibyte;
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
    for (const auto *property : {L"CanPickSourceFolder",
                                 L"IsWorkflowInteractionEnabled",
                                 L"CanRetryRecovery",
                                 L"IsStartupRecoveryPending",
                                 L"HasStartupRecoveryError",
                                 L"StartupRecoveryErrorMessage",
                                 L"CanRestoreStartupSourceAccess",
                                 L"StartupRecoveredTransactionCount",
                                 L"CanExportDiagnostics",
                                 L"HasDiagnosticLogError",
                                 L"SourceSelectionRegionVisibility",
                                 L"AnalysisRegionVisibility",
                                 L"ReviewRegionVisibility",
                                 L"ProcessingRegionVisibility",
                                 L"ResultsRegionVisibility",
                                 L"RecoveryRequiredRegionVisibility",
                                 L"ProcessingRowsRegionVisibility",
                                 L"BackupDestinationVisibility",
                                 L"OperationProgressVisibility",
                                 L"LandingRegionVisibility",
                                 L"WorkflowRegionVisibility",
                                 L"StartupRecoverySummaryVisibility",
                                 L"StartupRecoveryProgressVisibility",
                                 L"CanEditProcessingSettings",
                                 L"ProcessingSettingsSummary",
                                 L"MaximumConcurrentImageCountText",
                                 L"CustomMemoryBudgetVisibility",
                                 L"OutputDispositionDescription",
                                 L"HasNoPlannedCorrections"})
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
    // The new folder receives the user's defaults. They are not copied from a
    // previous batch's immutable review, identity or trimming acknowledgement.
    applyProcessingOptions(model.as<Presentation::MainWindowViewModel>(), processingOptionSelections());
    selectedSourceFolder_ = std::move(selectedFolder);
    batchIdentity_ = newIdentity;
    transactionEngine_ = composition->transactionEngine;
    transactionStartRequested_ = false;
    setViewModel(model.as<Presentation::MainWindowViewModel>());
    // The caller releases its picker gate before the one pending read-only
    // check starts. Settings changes are coalesced until workspace entry.
    automaticAnalysisPending_ = true;
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
    finishPickerInteraction();
}
void MainWindow::finishPickerInteraction()
{
    pickerActive_ = false;
    notifyShellState();
    analyzeOnWorkflowEntryAsync();
}
void MainWindow::synchronizeProcessingOptionControls()
{
    if (!viewModel_)
        return;
    updatingProcessingOptionControls_ = true;
    MaximumConcurrentImagesControl().Value(viewModel_.MaximumConcurrentImageCount());
    const auto memoryBudget = viewModel_.ProcessingMemoryBudgetBytes();
    AutomaticMemoryBudgetControl().IsOn(!memoryBudget);
    if (memoryBudget)
        ProcessingMemoryBudgetControl().Value(static_cast<double>(memoryBudget.Value()) /
                                              jpg_spinner::batch_processing::BatchExecutionLimits::bytesPerMebibyte);
    // Indices are local control positions, never native enum wire values.
    TraversalScopeControl().SelectedIndex(
        viewModel_.TraversalScope() == Presentation::TraversalScope::SelectedFolderOnly ? 0 : 1);
    using Disposition = Presentation::OutputDisposition;
    OutputDispositionControl().SelectedIndex(
        viewModel_.OutputDisposition() == Disposition::CreateCorrectedCopy
            ? 0
            : (viewModel_.OutputDisposition() == Disposition::ReplaceOriginalWithVerifiedBackup ? 1 : 2));
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
std::array<int, 4> MainWindow::processingOptionSelections()
{
    return {TraversalScopeControl().SelectedIndex(), OutputDispositionControl().SelectedIndex(),
            EdgeHandlingControl().SelectedIndex(), ScanOrganizationControl().SelectedIndex()};
}
void MainWindow::applyProcessingOptions(const Presentation::MainWindowViewModel &model,
                                        const std::array<int, 4> &selections)
{
    const auto [traversal, disposition, edge, scans] = selections;
    if (!areValidProcessingOptionSelections(selections))
        throw hresult_invalid_argument();
    using ScanOrganization = Presentation::OutputScanOrganization;
    const auto memoryBudget = selectedProcessingMemoryBudgetBytes(); // Validate before any review mutation.
    model.SetProcessingOptions(
        disposition == 0 ? Presentation::OutputDisposition::CreateCorrectedCopy
                         : (disposition == 1 ? Presentation::OutputDisposition::ReplaceOriginalWithVerifiedBackup
                                             : Presentation::OutputDisposition::ReplaceOriginalWithoutRetainedBackup),
        traversal == 0 ? Presentation::TraversalScope::SelectedFolderOnly
                       : Presentation::TraversalScope::SelectedFolderAndDescendants,
        edge == 0 ? Presentation::EdgeHandlingPolicy::RequirePerfectCoefficientTransform
                  : Presentation::EdgeHandlingPolicy::TrimPartialMinimumCodedUnits,
        scans == 0 ? ScanOrganization::PreserveSource
                   : (scans == 1 ? ScanOrganization::SequentialDct : ScanOrganization::ProgressiveDct));
    model.SetMaximumConcurrentImageCount(selectedMaximumConcurrentImageCount());
    model.SetProcessingMemoryBudget(
        memoryBudget ? box_value(*memoryBudget).as<Windows::Foundation::IReference<std::uint64_t>>() : nullptr);
}
void MainWindow::loadProcessingPreferences()
{
    // The composite is one native app-local snapshot. Versioned UI selections
    // are private persisted data, not native/domain enum wire values. Invalid
    // or older data leaves all four safe authored defaults unchanged.
    try
    {
        const auto values = Windows::Storage::ApplicationData::Current().LocalSettings().Values();
        const auto stored = values.TryLookup(L"ImageProcessingPreferences.v1")
                                .try_as<Windows::Storage::ApplicationDataCompositeValue>();
        if (!stored)
            return;
        std::array<int, 4> selections{};
        for (std::size_t index = 0; index < selections.size(); ++index)
            selections[index] =
                unbox_value_or<int32_t>(stored.TryLookup(hstring{processingPreferenceSelectionKeys[index]}), -1);
        if (!areValidProcessingOptionSelections(selections))
            return;
        TraversalScopeControl().SelectedIndex(selections[0]);
        OutputDispositionControl().SelectedIndex(selections[1]);
        EdgeHandlingControl().SelectedIndex(selections[2]);
        ScanOrganizationControl().SelectedIndex(selections[3]);
        const auto parallel = unbox_value_or<std::uint32_t>(stored.TryLookup(hstring{parallelImagePreferenceKey}),
                                                            selectedMaximumConcurrentImageCount());
        if (parallel >= 1 &&
            parallel <= jpg_spinner::batch_processing::BatchExecutionLimits::maximumSupportedConcurrentImageCount)
            MaximumConcurrentImagesControl().Value(
                (std::min)(static_cast<double>(parallel), MaximumConcurrentImagesControl().Maximum()));
        const auto custom = unbox_value_or<double>(stored.TryLookup(hstring{customMemoryPreferenceKey}),
                                                   ProcessingMemoryBudgetControl().Value());
        if (isPositiveWholeNumberWithin(custom, ProcessingMemoryBudgetControl().Maximum()))
        {
            ProcessingMemoryBudgetControl().Value(custom);
            AutomaticMemoryBudgetControl().IsOn(
                unbox_value_or<bool>(stored.TryLookup(hstring{automaticMemoryPreferenceKey}), true));
        }
    }
    catch (const hresult_error &)
    {
        SettingsSaveErrorBar().IsOpen(true);
    }
}
void MainWindow::saveProcessingPreferences(const std::array<int, 4> &selections)
{
    Windows::Storage::ApplicationDataCompositeValue preferences;
    for (std::size_t index = 0; index < selections.size(); ++index)
        preferences.Insert(hstring{processingPreferenceSelectionKeys[index]}, box_value(selections[index]));
    try
    {
        preferences.Insert(hstring{parallelImagePreferenceKey}, box_value(selectedMaximumConcurrentImageCount()));
        preferences.Insert(hstring{automaticMemoryPreferenceKey}, box_value(AutomaticMemoryBudgetControl().IsOn()));
        preferences.Insert(hstring{customMemoryPreferenceKey}, box_value(ProcessingMemoryBudgetControl().Value()));
        Windows::Storage::ApplicationData::Current().LocalSettings().Values().Insert(L"ImageProcessingPreferences.v1",
                                                                                     preferences);
        SettingsSaveErrorBar().IsOpen(false);
    }
    catch (const hresult_error &)
    {
        // Session choices remain usable, but failure to remember them must be
        // visible. No failure grants permission to change files or consent.
        SettingsSaveErrorBar().IsOpen(true);
    }
}
fire_and_forget MainWindow::ProcessingOptions_SelectionChanged(
    Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &)
{
    auto lifetime = get_strong();
    co_await applyProcessingPreferencesAsync();
}
fire_and_forget MainWindow::MaximumConcurrentImages_ValueChanged(
    Windows::Foundation::IInspectable const &,
    Microsoft::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const &)
{
    auto lifetime = get_strong();
    co_await applyProcessingPreferencesAsync();
}
fire_and_forget MainWindow::AutomaticMemoryBudget_Toggled(Windows::Foundation::IInspectable const &,
                                                          Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    co_await applyProcessingPreferencesAsync();
}
fire_and_forget MainWindow::ProcessingMemoryBudget_ValueChanged(
    Microsoft::UI::Xaml::Controls::NumberBox const &sender,
    Microsoft::UI::Xaml::Controls::NumberBoxValueChangedEventArgs const &arguments)
{
    auto lifetime = get_strong();
    if (!processingPreferencesReady_ || updatingProcessingOptionControls_ || !CanEditProcessingSettings())
        co_return;
    // Native NumberBox evaluates on Enter, focus loss or its spin buttons.
    // Keep the preference in whole MiB and restore an empty edit's last value;
    // neither a transient NaN nor a fractional edit reaches a reviewed batch.
    const auto entered = arguments.NewValue();
    auto accepted = std::isfinite(entered) ? std::clamp(std::round(entered), sender.Minimum(), sender.Maximum())
                                           : arguments.OldValue();
    if (!std::isfinite(accepted))
        accepted = sender.Minimum();
    if (entered != accepted)
    {
        sender.Value(accepted);
        co_return; // The resulting valid native ValueChanged applies the choice.
    }
    co_await applyProcessingPreferencesAsync();
}
Windows::Foundation::IAsyncAction MainWindow::applyProcessingPreferencesAsync()
{
    auto lifetime = get_strong();
    // XAML initialization can raise this before every named control exists.
    if (!processingPreferencesReady_ || updatingProcessingOptionControls_ || !CanEditProcessingSettings())
        co_return;
    try
    {
        const auto selections = processingOptionSelections();
        if (!areValidProcessingOptionSelections(selections))
            co_return; // Native selection can transiently clear during initialization.
        if (!viewModel_)
        {
            saveProcessingPreferences(selections);
            notifyShellState();
            co_return;
        }
        if (viewModel_.State() == Presentation::ImageProcessingExperienceState::Results)
        {
            pickerActive_ = true;
            notifyShellState();
            releaseReconciledSourceAccess();
            // Results' request belongs to an executed UUID. Compose a fresh
            // native batch before applying the user's newly selected choices.
            co_await selectPickedSourceAsync(selectedSourceFolder_);
        }
        applyProcessingOptions(viewModel_, selections);
        automaticAnalysisPending_ = viewModel_.CanAnalyzeSelectedSource();
        saveProcessingPreferences(selections);
        synchronizeProcessingOptionControls();
    }
    catch (...)
    {
        synchronizeProcessingOptionControls();
        showIntegrationError();
    }
    finishPickerInteraction();
}
fire_and_forget MainWindow::RetryAnalysis_Click(Windows::Foundation::IInspectable const &,
                                                Microsoft::UI::Xaml::RoutedEventArgs const &)
{
    auto lifetime = get_strong();
    co_await analyzeSelectedSourceAsync();
}
fire_and_forget MainWindow::analyzeOnWorkflowEntryAsync()
{
    // Initial navigation can occur in the constructor before a source exists.
    // Avoid acquiring/starting an asynchronous operation in that case.
    if (!automaticAnalysisPending_ || !viewModel_ || !viewModel_.CanAnalyzeSelectedSource())
        co_return;
    auto lifetime = get_strong();
    co_await analyzeSelectedSourceAsync();
}
Windows::Foundation::IAsyncAction MainWindow::analyzeSelectedSourceAsync()
{
    auto lifetime = get_strong();
    if (closeAdmission_.closeRequested() || pickerActive_ || operationEventActive_ || !viewModel_ ||
        !viewModel_.CanAnalyzeSelectedSource())
        co_return;
    const auto page = ShellFrame().Content().try_as<Microsoft::UI::Xaml::Controls::Page>();
    if (!page || unbox_value_or<hstring>(page.Tag(), L"") != L"Workflow")
        co_return; // Coalesce preference edits; navigation back starts one analysis.
    automaticAnalysisPending_ = false;
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
    finishPickerInteraction();
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
    finishPickerInteraction();
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
    finishPickerInteraction();
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
        automaticAnalysisPending_ = viewModel_.CanAnalyzeSelectedSource();
    }
    catch (...)
    {
        showIntegrationError();
    }
    finishPickerInteraction();
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
