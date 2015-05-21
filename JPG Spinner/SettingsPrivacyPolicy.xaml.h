//
// SettingsPrivacyPolicy.xaml.h
// Declaration of the SettingsPrivacyPolicy class
//

#pragma once

#include "SettingsPrivacyPolicy.g.h"
#include "MainPage.xaml.h"

namespace JPG_Spinner
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class SettingsPrivacyPolicy sealed
	{
	public:
		SettingsPrivacyPolicy();

	private:
		bool _navigationShortcutsRegistered;
		Windows::Foundation::EventRegistrationToken _acceleratorKeyEventToken;
		MainPage^ rootPage;

		void SettingsPrivacyPolicy_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SettingsPrivacyPolicy_Unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void SettingsPrivacyPolicy_AcceleratorKeyActivated(Windows::UI::Core::CoreDispatcher^ sender, Windows::UI::Core::AcceleratorKeyEventArgs^ args);	

		void PrivacyPolicyDate_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void HyperlinkPrivacyPolicy_Click(Windows::UI::Xaml::Documents::Hyperlink^ sender, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs^ args);
	};
}