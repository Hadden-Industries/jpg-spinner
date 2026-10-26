#include "pch.h"
#include "ShellContentPage.xaml.h"
#include "ShellContentPage.g.cpp"

namespace winrt::JpgSpinner::implementation
{
ShellContentPage::ShellContentPage()
{
    InitializeComponent();
}

void ShellContentPage::OnNavigatedFrom(const Microsoft::UI::Xaml::Navigation::NavigationEventArgs &)
{
    // Detach the composed view before a later Back/Forward visit reparents it.
    // The window retains the view and its controls, so choices, carousel position
    // and the processing model survive navigation without another state store.
    Content(nullptr);
}
} // namespace winrt::JpgSpinner::implementation
