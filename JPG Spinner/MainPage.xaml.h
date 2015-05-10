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

		property BOOL CropChecked;
		property BOOL ProgressiveChecked;
		property BOOL DeleteThumbnails;
		property Platform::IBox<intptr_t>^ cts
		{
			Platform::IBox<intptr_t>^ get()
			{
				return ref new Platform::Box<intptr_t>((intptr_t)&_cts);
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

		void ShowDebugText(Platform::String^ debugText);
		void Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void TextBlockCrop_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void TextBlockProgressive_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void HyperLinkButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void ButtonSelectFiles_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CheckBoxProgressive_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CheckBoxProgressive_Unchecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CheckBoxCrop_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CheckBoxCrop_Unchecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		concurrency::task<bool> SaveSettingAsync(Platform::String^ key, Platform::String^ value);
		concurrency::task<Platform::String^> LoadSettingAsync(Platform::String^ key);
		void CheckBoxProgressive_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void CheckBoxCrop_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void Logo_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
	};
}
