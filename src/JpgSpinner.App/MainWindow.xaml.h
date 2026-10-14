#pragma once

#include "MainWindow.g.h"
#include <winrt/JpgSpinner.Presentation.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Storage.h>
#include <jpg_spinner/batch/BatchProcessingCoordinator.h>
#include <jpg_spinner/storage/WindowsStorageImageFileTransactionEngine.h>
#include "Composition/SourceFolderAccessRetention.h"
#include <optional>

namespace winrt::JpgSpinner::implementation
{
// winrt::make uses generated heap implementation machinery that derives from
// this runtime-class implementation, so the implementation cannot be final.
struct MainWindow : MainWindowT<MainWindow>
{
    MainWindow();
    Presentation::MainWindowViewModel ViewModel() const;
    bool CanPickSourceFolder() const;
    bool IsWorkflowInteractionEnabled() const;
    bool CanRetryRecovery() const;
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
    Presentation::MainWindowViewModel viewModel_{nullptr};
    Microsoft::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker viewModelSubscription_;
    Microsoft::UI::Xaml::XamlRoot::Changed_revoker xamlRootChangedSubscription_;
    event<Microsoft::UI::Xaml::Data::PropertyChangedEventHandler> propertyChanged_;
    bool pickerActive_{};
    bool operationEventActive_{};
    bool updatingProcessingOptionControls_{};
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
