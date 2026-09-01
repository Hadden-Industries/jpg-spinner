#include "pch.h"

#include "App.xaml.h"
#include "MainWindow.xaml.h"

namespace winrt::JpgSpinner::implementation
{
App::App()
{
#if defined(_DEBUG) && !defined(DISABLE_XAML_GENERATED_BREAK_ON_UNHANDLED_EXCEPTION)
    // Match Microsoft's template behavior: break only while an attached
    // debugger can make the otherwise-unhandled XAML failure actionable.
    UnhandledException([](auto const&, Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& exceptionArguments) {
        if (IsDebuggerPresent())
        {
            [[maybe_unused]] auto const errorMessage = exceptionArguments.Message();
            __debugbreak();
        }
    });
#endif
}
void App::OnLaunched([[maybe_unused]] Microsoft::UI::Xaml::LaunchActivatedEventArgs const& launchArguments)
{
    mainWindow = winrt::make<MainWindow>();
    mainWindow.Activate();
}
}
