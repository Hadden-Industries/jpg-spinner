//
// ExplanationProgressive.xaml.h
// Declaration of the ExplanationProgressive class
//

#pragma once

#include "ExplanationProgressive.g.h"

namespace JPG_Spinner
{
	[Windows::UI::Xaml::Data::Bindable]
	[Windows::Foundation::Metadata::WebHostHiddenAttribute]
	public ref class ProgressiveDataItem sealed
	{
		Platform::String^ _caption;
		Windows::UI::Xaml::Media::Imaging::WriteableBitmap^ _image;

		event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ _PropertyChanged;

	public:
		ProgressiveDataItem(Platform::String^ caption, Windows::UI::Xaml::Media::Imaging::WriteableBitmap^ image);

		void OnPropertyChanged(Platform::String^ propertyName)
		{
			Windows::UI::Xaml::Data::PropertyChangedEventArgs^ pcea = ref new  Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName);
			_PropertyChanged(this, pcea);
		}

		// Caption
		property Platform::String^ Caption
		{
			Platform::String^ get()
			{
				return _caption;
			}
			void set(Platform::String^ value)
			{
				_caption = value;
				OnPropertyChanged("Caption");
			}
		}

		// Image
		property Windows::UI::Xaml::Media::Imaging::WriteableBitmap^ Image
		{
			Windows::UI::Xaml::Media::Imaging::WriteableBitmap^ get()
			{
				return _image;
			}
			void set(Windows::UI::Xaml::Media::Imaging::WriteableBitmap^ value)
			{
				_image = value;
				OnPropertyChanged("Image");
			}
		}
	};

	[Windows::Foundation::Metadata::WebHostHiddenAttribute]
	public ref class ProgressiveData sealed
	{
		Windows::UI::Xaml::Interop::IBindableObservableVector^ _items;

	public:
		ProgressiveData()
		{
			_items = ref new Platform::Collections::Vector<ProgressiveDataItem^>();
		}

		property Windows::UI::Xaml::Interop::IBindableObservableVector^ Items
		{
			Windows::UI::Xaml::Interop::IBindableObservableVector^ get()
			{
				return _items;
			}
		}
	};

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

	private:
		Windows::UI::Core::CoreDispatcher^ _dispatcher;
		Windows::ApplicationModel::Resources::ResourceLoader^ _resourceLoader;

		ProgressiveData^ data;
		
		void FlipView1_SizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
	};
}