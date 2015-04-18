//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"

namespace JPG_Spinner
{
	/// <summary>
	/// Provides application-specific behavior to supplement the default Application class.
	/// </summary>
	ref class App sealed
	{
	protected:
		virtual void OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e) override;

	internal:
		App();

	private:
		void OnSuspending(Platform::Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void OnNavigationFailed(Platform::Object ^sender, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs ^e);
	};

	[Windows::UI::Xaml::Data::Bindable]
	[Windows::Foundation::Metadata::WebHostHiddenAttribute]
	public ref class Item sealed
	{
		Windows::Storage::StorageFile^ _StorageFile;
		uint32 _ID;
		Platform::String^ _MRUToken;
		Platform::String^  _Title;
		Platform::String^  _Error;
		Windows::UI::Xaml::Media::ImageSource^ _Image;
		unsigned short _OrientationFlag;

		event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ _PropertyChanged;

	public:
		Item();

		void OnPropertyChanged(Platform::String^ propertyName)
		{
			Windows::UI::Xaml::Data::PropertyChangedEventArgs^ pcea = ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName);
			_PropertyChanged(this, pcea);
		}

		//StorageFile
		property Windows::Storage::StorageFile^ StorageFile
		{
			Windows::Storage::StorageFile^ get()
			{
				return _StorageFile;
			}
			void set(Windows::Storage::StorageFile^ value)
			{
				_StorageFile = value;
				OnPropertyChanged("StorageFile");
			}
		}

		//ID
		property uint32 ID
		{
			uint32 get()
			{
				return _ID;
			}
			void set(uint32 value)
			{
				_ID = value;
				OnPropertyChanged("ID");
			}
		}

		//MRUToken
		property Platform::String^ MRUToken
		{
			Platform::String^ get()
			{
				return _MRUToken;
			}
			void set(Platform::String^ value)
			{
				_MRUToken = value;
				OnPropertyChanged("MRUToken");
			}
		}

		//Title
		property Platform::String^ Title
		{
			Platform::String^ get()
			{
				return _Title;
			}
			void set(Platform::String^ value)
			{
				_Title = value;
				OnPropertyChanged("Title");
			}
		}

		//Error
		property Platform::String^ Error
		{
			Platform::String^ get()
			{
				return _Error;
			}
			void set(Platform::String^ value)
			{
				_Error = value;
				OnPropertyChanged("Error");
			}
		}

		//Image
		property Windows::UI::Xaml::Media::ImageSource^ Image
		{
			Windows::UI::Xaml::Media::ImageSource^ get()
			{
				return _Image;
			}
			void set(Windows::UI::Xaml::Media::ImageSource^ value)
			{
				_Image = value;
				OnPropertyChanged("Image");
			}
		}

		//OrientationFlag
		property unsigned short OrientationFlag
		{
			unsigned short get()
			{
				return _OrientationFlag;
			}
			void set(unsigned short value)
			{
				_OrientationFlag = value;
				OnPropertyChanged("OrientationFlag");
			}
		}
	};

	[Windows::Foundation::Metadata::WebHostHiddenAttribute]
	public ref class Data sealed
	{
		Windows::UI::Xaml::Interop::IBindableObservableVector^  _items;

	public:
		Data()
		{
			_items = ref new Platform::Collections::Vector<Item^>();
		}

		property Windows::UI::Xaml::Interop::IBindableObservableVector^ Items
		{
			Windows::UI::Xaml::Interop::IBindableObservableVector^ get()
			{
				return _items;
			}

		}
	};
}
