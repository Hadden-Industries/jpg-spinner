//
// ExplanationProgressive.xaml.h
// Declaration of the ExplanationProgressive class
//

#pragma once

#include "ExplanationProgressive.g.h"

namespace JPG_Spinner
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class ExplanationProgressive sealed
	{
	public:
		ExplanationProgressive();

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

	private:
		Windows::UI::Core::CoreDispatcher^ _dispatcher;
		Windows::ApplicationModel::Resources::ResourceLoader^ _resourceLoader;

		Platform::Collections::Vector<Windows::UI::Xaml::Media::Imaging::WriteableBitmap^>^ vector;
		UINT currentLevel;
		Windows::Foundation::EventRegistrationToken keyDownToken;

		void CoreWindow_KeyDown(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::KeyEventArgs^ args);

		void IncreaseProgressiveLevel(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void DecreaseProgressiveLevel(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);

		void ButtonLeftTextBlock_PointerEntered(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void ButtonLeftTextBlock_PointerExited(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);

		void ButtonRightTextBlock_PointerEntered(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
		void ButtonRightTextBlock_PointerExited(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
	};
}