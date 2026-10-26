#pragma once

#include "ShellContentPage.g.h"
#include <winrt/Microsoft.UI.Xaml.Navigation.h>

namespace winrt::JpgSpinner::implementation
{
/// A real Frame destination for the window's composed views. The Frame journal
/// retains only a small destination identifier, never a view/model reference.
struct ShellContentPage : ShellContentPageT<ShellContentPage>
{
    ShellContentPage();
    void OnNavigatedFrom(const Microsoft::UI::Xaml::Navigation::NavigationEventArgs &);
};
} // namespace winrt::JpgSpinner::implementation

namespace winrt::JpgSpinner::factory_implementation
{
struct ShellContentPage : ShellContentPageT<ShellContentPage, implementation::ShellContentPage>
{
};
} // namespace winrt::JpgSpinner::factory_implementation
