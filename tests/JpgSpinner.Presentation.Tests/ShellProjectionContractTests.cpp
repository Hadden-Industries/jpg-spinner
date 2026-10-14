#include "pch.h"
#include <winrt/JpgSpinner.h>
#include <winrt/JpgSpinner.Presentation.h>
#include <winrt/Microsoft.UI.Xaml.Data.h>
#include <catch2/catch_test_macros.hpp>
#include <concepts>
#include <utility>

TEST_CASE("Window metadata exposes typed observable shell bindings", "[presentation][metadata][shell]")
{
    using Window = winrt::JpgSpinner::MainWindow;
    using Model = winrt::JpgSpinner::Presentation::MainWindowViewModel;
    using Visibility = winrt::Microsoft::UI::Xaml::Visibility;
    // Exercise the owning MIDL/cppwinrt consumer output, not handwritten ABI
    // declarations. No Window activation, deployment or UI tree is claimed.
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().ViewModel()), Model>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().CanPickSourceFolder()), bool>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().IsWorkflowInteractionEnabled()), bool>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().CanRetryRecovery()), bool>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().IsStartupRecoveryPending()), bool>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().HasStartupRecoveryError()), bool>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().CanRestoreStartupSourceAccess()), bool>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().StartupRecoveredTransactionCount()), winrt::hstring>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().SourceSelectionRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().AnalysisRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().ReviewRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().ProcessingRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().ResultsRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().RecoveryRequiredRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().ProcessingRowsRegionVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().BackupDestinationVisibility()), Visibility>);
    STATIC_REQUIRE(std::same_as<decltype(std::declval<Window>().OperationProgressVisibility()), Visibility>);
    CHECK(std::is_convertible_v<Window, winrt::Microsoft::UI::Xaml::Data::INotifyPropertyChanged>);
}
