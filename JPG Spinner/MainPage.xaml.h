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

		// Read-write property
		/*property Platform::Collections::Vector<Windows::Storage::StorageFile^>^ PickedFiles
		{
			Platform::Collections::Vector<Windows::Storage::StorageFile^>^ get() { return _pickedFiles; }
			void set(Platform::Collections::Vector<Windows::Storage::StorageFile^>^ value)
			{
				_pickedFiles = value;
			}
		}*/

	private:
		void Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	
	internal:
		static MainPage^ Current;		
	};	
}
