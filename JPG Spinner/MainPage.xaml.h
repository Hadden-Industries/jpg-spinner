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

		property BOOL TrimChecked;
		property BOOL ProgressiveChecked;
		property Platform::IBox<int64>^ cts
		{
			Platform::IBox<int64>^ get()
			{
				return ref new Platform::Box<int64>((int64)&_cts);
			}
		};

		void SpinLogo_Start();
		void SpinLogo_Stop();
		void FlipButton();

	internal:
		static MainPage^ Current;

	private:
		concurrency::cancellation_token_source _cts;
		volatile std::atomic_bool buttonIsSelectFiles;

		void Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void TextBlockTrim_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void TextBlockProgressive_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void HyperLinkButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ButtonPickFiles_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
