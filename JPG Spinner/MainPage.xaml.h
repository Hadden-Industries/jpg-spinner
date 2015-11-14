//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

namespace JPG_Spinner
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();

		property Platform::IBox<intptr_t>^ cts
		{
			Platform::IBox<intptr_t>^ get()
			{
				return ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&_cts));
			}
		};

		void SpinLogo_Start();
		void SpinLogo_Stop();
		void FlipButton();
		void LoadWebPage(Platform::String^ uri);
		void CancelProcessing();
		void Navigate(Windows::UI::Xaml::Interop::TypeName type, Platform::Object^ obj);

	internal:
		static MainPage^ Current;

	private:
		Concurrency::cancellation_token_source _cts;
		volatile std::atomic_bool buttonIsSelectFiles;

		void ShowDebugText(Platform::String^ debugText);
		void Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		
		void HyperLinkButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ButtonSelectFiles_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void Logo_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
	};
}
