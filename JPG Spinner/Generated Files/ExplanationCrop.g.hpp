﻿

//------------------------------------------------------------------------------
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
//------------------------------------------------------------------------------
#include "pch.h"
#include "ExplanationCrop.xaml.h"




void ::JPG_Spinner::ExplanationCrop::InitializeComponent()
{
    if (_contentLoaded)
        return;

    _contentLoaded = true;

    // Call LoadComponent on ms-appx:///ExplanationCrop.xaml
    ::Windows::UI::Xaml::Application::LoadComponent(this, ref new ::Windows::Foundation::Uri(L"ms-appx:///ExplanationCrop.xaml"), ::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Application);

    // Get the Grid named 'LayoutRoot'
    LayoutRoot = safe_cast<::Windows::UI::Xaml::Controls::Grid^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"LayoutRoot"));
    // Get the Grid named 'Input'
    Input = safe_cast<::Windows::UI::Xaml::Controls::Grid^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"Input"));
    // Get the Grid named 'Output'
    Output = safe_cast<::Windows::UI::Xaml::Controls::Grid^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"Output"));
    // Get the Image named 'Image1'
    Image1 = safe_cast<::Windows::UI::Xaml::Controls::Image^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"Image1"));
    // Get the TextBlock named 'TextBlock1'
    TextBlock1 = safe_cast<::Windows::UI::Xaml::Controls::TextBlock^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"TextBlock1"));
    // Get the TextBlock named 'InputTextBlock1'
    InputTextBlock1 = safe_cast<::Windows::UI::Xaml::Controls::TextBlock^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"InputTextBlock1"));
}

void ::JPG_Spinner::ExplanationCrop::Connect(int connectionId, Platform::Object^ target)
{
    (void)connectionId; // Unused parameter
    (void)target; // Unused parameter
    _contentLoaded = true;
}

