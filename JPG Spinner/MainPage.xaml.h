//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

HRESULT GetJPEGOrientationFlag(LPCWSTR FileName, USHORT &OrientationFlag, IWICImagingFactory * pIWICImagingFactory);
HRESULT SetJPEGOrientationFlag(LPCWSTR FileName, const USHORT OrientationFlag, IWICImagingFactory * pIWICImagingFactory);

namespace JPG_Spinner
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();

	private:
		~MainPage();
		IWICImagingFactory * pIWICImagingFactory;
		void Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		Platform::String^ mruToken;
	};
}
