#pragma once

#include "MainWindow.g.h"
#include <winrt/JpgSpinner.Presentation.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Storage.h>
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "Composition/SourceFolderAccessRetention.h"
#include "Composition/CooperativeWindowClose.h"
#include "Diagnostics/LocalDiagnosticLog.h"
#include <winrt/Microsoft.UI.Windowing.h>
#include <optional>

namespace winrt::JpgSpinner::implementation
{
// winrt::make uses generated heap implementation machinery that derives from
// this runtime-class implementation, so the implementation cannot be final.
struct MainWindow : MainWindowT<MainWindow>
{
    MainWindow();
    ~MainWindow();
    Presentation::MainWindowViewModel ViewModel() const;
    bool CanPickSourceFolder() const;
    bool IsWorkflowInteractionEnabled() const;
    bool CanRetryRecovery() const;
    bool IsStartupRecoveryPending() const;
    bool HasStartupRecoveryError() const;
    /// Localized preservation context and the actual recovery refusal's remedy.
    winrt::hstring StartupRecoveryErrorMessage() const;
    bool CanRestoreStartupSourceAccess() const;
    bool CanExportDiagnostics() const;
    bool HasDiagnosticLogError() const;
    winrt::hstring StartupRecoveredTransactionCount() const;
    Microsoft::UI::Xaml::Visibility SourceSelectionRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility AnalysisRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility ReviewRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility ProcessingRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility ResultsRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility RecoveryRequiredRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility ProcessingRowsRegionVisibility() const;
    Microsoft::UI::Xaml::Visibility BackupDestinationVisibility() const;
    Microsoft::UI::Xaml::Visibility OperationProgressVisibility() const;
    fire_and_forget SourcePicker_Click(Windows::Foundation::IInspectable const &,
                                       Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget Analyze_Click(Windows::Foundation::IInspectable const &,
                                  Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget StartProcessing_Click(Windows::Foundation::IInspectable const &,
                                          Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget RetryRecovery_Click(Windows::Foundation::IInspectable const &,
                                        Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget RestoreRecoverySource_Click(Windows::Foundation::IInspectable const &,
                                                Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget ExportDiagnostics_Click(Windows::Foundation::IInspectable const &,
                                            Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget ReturnToSourceSelection_Click(Windows::Foundation::IInspectable const &,
                                                  Microsoft::UI::Xaml::RoutedEventArgs const &);
    void CancelOperation_Click(Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::RoutedEventArgs const &);
    void TrimConsent_Click(Windows::Foundation::IInspectable const &sender,
                           Microsoft::UI::Xaml::RoutedEventArgs const &);
    fire_and_forget ProcessingOptions_SelectionChanged(
        Windows::Foundation::IInspectable const &, Microsoft::UI::Xaml::Controls::SelectionChangedEventArgs const &);
    event_token PropertyChanged(Microsoft::UI::Xaml::Data::PropertyChangedEventHandler const &handler);
    void PropertyChanged(event_token const &token);

  private:
    /// Applies the effective-pixel UX minimum through the native presenter at
    /// the current XamlRoot display scale. Requires the loaded UI-thread root.
    void updatePreferredMinimumWindowSize();
    void requireUiThread() const;
    Microsoft::UI::Xaml::Visibility regionVisibility(Presentation::ImageProcessingExperienceState state) const;
    void setViewModel(Presentation::MainWindowViewModel const &viewModel);
    void notifyShellState();
    Windows::Foundation::IAsyncAction selectPickedSourceAsync(Windows::Storage::StorageFolder selectedFolder);
    void synchronizeProcessingOptionControls();
    void showIntegrationError();
    void retainSelectedSourceForTransaction();
    void releaseReconciledSourceAccess();
    void rememberCompletedBatchFolders();
    /// Keeps normal selection gated until native retained-grant recovery has
    /// completed. Blocking storage consumers run only on the owned MTA worker.
    Windows::Foundation::IAsyncAction recoverStartupAsync();
    /// Only ordinary window close can be deferred. Forced process/session
    /// termination still relies on the persisted transaction recovery contract.
    void handleClosing(const Microsoft::UI::Windowing::AppWindowClosingEventArgs &arguments);
    bool hasActiveShellWork() const;
    void closeAfterWorkSettles();
    Presentation::MainWindowViewModel viewModel_{nullptr};
    Microsoft::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker viewModelSubscription_;
    Microsoft::UI::Xaml::XamlRoot::Changed_revoker xamlRootChangedSubscription_;
    // AppWindow does not support the weak reference required by auto_revoke.
    // A native event token plus a weak receiver avoids both a crash and a cycle.
    Microsoft::UI::Windowing::AppWindow closingEventSource_{nullptr};
    winrt::event_token closingEventToken_{};
    event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged_;
    bool pickerActive_{};
    bool operationEventActive_{};
    bool updatingProcessingOptionControls_{};
    bool startupRecoveryPending_{true};
    bool startupRecoveryStarted_{};
    std::optional<jpg_spinner::domain::ImageProcessingError> startupRecoveryError_;
    std::size_t startupRecoveredTransactionCount_{};
    jpg_spinner::presentation::CooperativeWindowClose closeAdmission_;
    Windows::Foundation::IAsyncInfo activeOperation_{nullptr};
    std::stop_source startupRecoveryCancellation_;
    bool deferredCloseDispatched_{};
    std::shared_ptr<jpg_spinner::diagnostics::LocalDiagnosticLog> diagnosticLog_;
    bool diagnosticLogError_{};
    Windows::Storage::StorageFolder selectedSourceFolder_{nullptr};
    guid batchIdentity_{};
    std::optional<jpg_spinner::presentation::SourceFolderAccessRetention> sourceAccessRetention_;
    bool transactionStartRequested_{};
    std::shared_ptr<const jpg_spinner::storage::WindowsStorageImageFileTransactionEngine> transactionEngine_;
    std::vector<jpg_spinner::storage::IdentityBoundStorageFolder> applicationOwnedFolders_;
    std::shared_ptr<const jpg_spinner::batch_processing::BatchProcessingCoordinator> coordinator_;
};
} // namespace winrt::JpgSpinner::implementation

namespace winrt::JpgSpinner::factory_implementation
{
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};
} // namespace winrt::JpgSpinner::factory_implementation
