﻿

//------------------------------------------------------------------------------
//     This code was generated by a tool.
//
//     Changes to this file may cause incorrect behavior and will be lost if
//     the code is regenerated.
//------------------------------------------------------------------------------
#include "pch.h"
#include "SettingsPrivacyPolicy.xaml.h"




void ::JPG_Spinner::SettingsPrivacyPolicy::InitializeComponent()
{
    if (_contentLoaded)
        return;

    _contentLoaded = true;

    // Call LoadComponent on ms-appx:///SettingsPrivacyPolicy.xaml
    ::Windows::UI::Xaml::Application::LoadComponent(this, ref new ::Windows::Foundation::Uri(L"ms-appx:///SettingsPrivacyPolicy.xaml"), ::Windows::UI::Xaml::Controls::Primitives::ComponentResourceLocation::Application);

    // Get the TextBlock named 'PrivacyPolicyDate'
    PrivacyPolicyDate = safe_cast<::Windows::UI::Xaml::Controls::TextBlock^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"PrivacyPolicyDate"));
    // Get the Paragraph named 'PrivacyPolicyParagraph1'
    PrivacyPolicyParagraph1 = safe_cast<::Windows::UI::Xaml::Documents::Paragraph^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"PrivacyPolicyParagraph1"));
    // Get the Paragraph named 'PrivacyPolicyParagraph2'
    PrivacyPolicyParagraph2 = safe_cast<::Windows::UI::Xaml::Documents::Paragraph^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"PrivacyPolicyParagraph2"));
    // Get the Paragraph named 'PrivacyPolicyParagraph3'
    PrivacyPolicyParagraph3 = safe_cast<::Windows::UI::Xaml::Documents::Paragraph^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"PrivacyPolicyParagraph3"));
    // Get the Paragraph named 'PrivacyPolicyParagraph4'
    PrivacyPolicyParagraph4 = safe_cast<::Windows::UI::Xaml::Documents::Paragraph^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"PrivacyPolicyParagraph4"));
    // Get the Hyperlink named 'HyperlinkAnalyzingCrashReports'
    HyperlinkAnalyzingCrashReports = safe_cast<::Windows::UI::Xaml::Documents::Hyperlink^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"HyperlinkAnalyzingCrashReports"));
    // Get the Hyperlink named 'HyperlinkPrivacyPolicy'
    HyperlinkPrivacyPolicy = safe_cast<::Windows::UI::Xaml::Documents::Hyperlink^>(static_cast<Windows::UI::Xaml::IFrameworkElement^>(this)->FindName(L"HyperlinkPrivacyPolicy"));
}

void ::JPG_Spinner::SettingsPrivacyPolicy::Connect(int connectionId, Platform::Object^ target)
{
    switch (connectionId)
    {
    case 1:
        (safe_cast<::Windows::UI::Xaml::FrameworkElement^>(target))->Loaded +=
            ref new ::Windows::UI::Xaml::RoutedEventHandler(this, (void (::JPG_Spinner::SettingsPrivacyPolicy::*)(Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^))&SettingsPrivacyPolicy::SettingsPrivacyPolicy_Loaded);
        (safe_cast<::Windows::UI::Xaml::FrameworkElement^>(target))->Unloaded +=
            ref new ::Windows::UI::Xaml::RoutedEventHandler(this, (void (::JPG_Spinner::SettingsPrivacyPolicy::*)(Platform::Object^, Windows::UI::Xaml::RoutedEventArgs^))&SettingsPrivacyPolicy::SettingsPrivacyPolicy_Unloaded);
        break;
    case 2:
        (safe_cast<::Windows::UI::Xaml::Documents::Hyperlink^>(target))->Click +=
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::UI::Xaml::Documents::Hyperlink^, ::Windows::UI::Xaml::Documents::HyperlinkClickEventArgs^>(this, (void (::JPG_Spinner::SettingsPrivacyPolicy::*)(Windows::UI::Xaml::Documents::Hyperlink^, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs^))&SettingsPrivacyPolicy::HyperlinkPrivacyPolicy_Click);
        break;
    case 3:
        (safe_cast<::Windows::UI::Xaml::Documents::Hyperlink^>(target))->Click +=
            ref new ::Windows::Foundation::TypedEventHandler<::Windows::UI::Xaml::Documents::Hyperlink^, ::Windows::UI::Xaml::Documents::HyperlinkClickEventArgs^>(this, (void (::JPG_Spinner::SettingsPrivacyPolicy::*)(Windows::UI::Xaml::Documents::Hyperlink^, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs^))&SettingsPrivacyPolicy::HyperlinkPrivacyPolicy_Click);
        break;
    }
    (void)connectionId; // Unused parameter
    (void)target; // Unused parameter
    _contentLoaded = true;
}

