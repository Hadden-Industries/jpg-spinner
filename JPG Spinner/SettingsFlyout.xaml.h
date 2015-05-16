//
// SettingsFlyout.xaml.h
// Declaration of the SettingsFlyout class
//

#pragma once

#include "SettingsFlyout.g.h"

namespace JPG_Spinner
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class SettingsFlyout sealed
	{
	public:
		SettingsFlyout();

	private:
		bool _navigationShortcutsRegistered;
		Windows::Foundation::EventRegistrationToken _acceleratorKeyEventToken;

		void SettingsFlyout_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void SettingsFlyout_Unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void SettingsFlyout_AcceleratorKeyActivated(Windows::UI::Core::CoreDispatcher^ sender, Windows::UI::Core::AcceleratorKeyEventArgs^ args);	
	};
}