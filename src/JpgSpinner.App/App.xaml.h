#pragma once

#include "App.xaml.g.h"

namespace winrt::JpgSpinner::implementation
{
// winrt::make uses generated heap implementation machinery that derives from
// this runtime-class implementation, so the implementation must remain
// inheritable.
struct App : AppT<App>
{
    App();

    void OnLaunched(Microsoft::UI::Xaml::LaunchActivatedEventArgs const& launchArguments);

private:
    Microsoft::UI::Xaml::Window mainWindow{nullptr};
};
}
