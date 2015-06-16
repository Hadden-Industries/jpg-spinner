//
// SettingsFlyout.xaml.h
// Declaration of the SettingsFlyout class
//

#pragma once

#include "SettingsFlyout.g.h"
#include "MainPage.xaml.h"

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

		void TextBlockProgressive_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);

		void TextBlockCrop_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);

		void ToggleSwitch_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}