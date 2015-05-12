//
// App.xaml.h
// Declaration of the App class.
//

#pragma once

#include "App.g.h"
#include <Propvarutil.h> // PropVariantCompare

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
		Platform::Guid _UUID;
		Platform::String^ _MRUToken;
		Platform::String^  _Error;
		Windows::UI::Xaml::Media::ImageSource^ _Image;
		unsigned char _Orientation;
		unsigned char _OrientationXMP;
		unsigned short _JPEGInterchangeFormat;
		unsigned short _JPEGInterchangeFormatLength;
		PROPVARIANT _SubjectArea;
		PROPVARIANT _SubjectLocation;
		PROPVARIANT _FocalPlaneXResolution;
		PROPVARIANT _FocalPlaneYResolution;
		bool _HasThumbnail;

		event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ _PropertyChanged;

	public:
		Item();

		void OnPropertyChanged(Platform::String^ propertyName)
		{
			if (nullptr != propertyName)
			{
				Windows::UI::Xaml::Data::PropertyChangedEventArgs^ pcea = ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs(propertyName);
				_PropertyChanged(this, pcea);
			}
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
				if (nullptr == _StorageFile || !_StorageFile->Equals(value))
				{
					_StorageFile = value;
					OnPropertyChanged("StorageFile");

					if (nullptr == _StorageFile || !_StorageFile->DisplayName->Equals(value->DisplayName))
					{
						OnPropertyChanged("Title");
					}
				}
			}
		}

		//UUID
		property Platform::Guid UUID
		{
			Platform::Guid get()
			{
				return _UUID;
			}
			void set(Platform::Guid value)
			{
				if (_UUID != value)
				{
					_UUID = value;
					OnPropertyChanged("UUID");
				}
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
				if (!_MRUToken->Equals(value))
				{
					_MRUToken = value;
					OnPropertyChanged("MRUToken");
				}
			}
		}

		//Title
		property Platform::String^ Title
		{
			Platform::String^ get()
			{
				return _StorageFile->DisplayName;
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
				if (!_Error->Equals(value))
				{
					_Error = value;
					OnPropertyChanged("Error");
				}
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
				if (nullptr == _Image || !_Image->Equals(value))
				{
					_Image = value;
					OnPropertyChanged("Image");
				}
			}
		}

		//Orientation
		property unsigned char Orientation
		{
			unsigned char get()
			{
				return _Orientation;
			}
			void set(unsigned char value)
			{
				if (_Orientation != value)
				{
					_Orientation = value;
					OnPropertyChanged("Orientation");
				}
			}
		}

		//OrientationXMP
		property unsigned char OrientationXMP
		{
			unsigned char get()
			{
				return _OrientationXMP;
			}
			void set(unsigned char value)
			{
				if (_OrientationXMP != value)
				{
					_OrientationXMP = value;
					OnPropertyChanged("OrientationXMP");
				}
			}
		}

		//JPEGInterchangeFormat
		property unsigned short JPEGInterchangeFormat
		{
			unsigned short get()
			{
				return _JPEGInterchangeFormat;
			}
			void set(unsigned short value)
			{
				if (_JPEGInterchangeFormat != value)
				{
					_JPEGInterchangeFormat = value;
					OnPropertyChanged("JPEGInterchangeFormat");
				}
			}
		}

		//JPEGInterchangeFormatLength
		property unsigned short JPEGInterchangeFormatLength
		{
			unsigned short get()
			{
				return _JPEGInterchangeFormatLength;
			}
			void set(unsigned short value)
			{
				if (_JPEGInterchangeFormatLength != value)
				{
					_JPEGInterchangeFormatLength = value;
					OnPropertyChanged("JPEGInterchangeFormatLength");
				}
			}
		}

		//PtrSubjectArea
		property Platform::IBox<intptr_t>^ PtrSubjectArea
		{
			Platform::IBox<intptr_t>^ get()
			{
				return ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&_SubjectArea));
			}
			void set(Platform::IBox<intptr_t>^ ptrValue)
			{
				PROPVARIANT value = *(reinterpret_cast<PROPVARIANT*>(ptrValue->Value));

				if (0 != PropVariantCompare(_SubjectArea, value))
				{
					HRESULT hr = PropVariantCopy(&_SubjectArea, &value);

					if (SUCCEEDED(hr))
					{
						OnPropertyChanged("PtrSubjectArea");
					}
				}
			}
		};

		//PtrSubjectLocation
		property Platform::IBox<intptr_t>^ PtrSubjectLocation
		{
			Platform::IBox<intptr_t>^ get()
			{
				return ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&_SubjectLocation));
			}
			void set(Platform::IBox<intptr_t>^ ptrValue)
			{
				PROPVARIANT value = *(reinterpret_cast<PROPVARIANT*>(ptrValue->Value));

				if (0 != PropVariantCompare(_SubjectLocation, value))
				{
					HRESULT hr = PropVariantCopy(&_SubjectLocation, &value);

					if (SUCCEEDED(hr))
					{
						OnPropertyChanged("PtrSubjectLocation");
					}
				}
			}
		};

		//PtrFocalPlaneXResolution
		property Platform::IBox<intptr_t>^ PtrFocalPlaneXResolution
		{
			Platform::IBox<intptr_t>^ get()
			{
				return ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&_FocalPlaneXResolution));
			}
			void set(Platform::IBox<intptr_t>^ ptrValue)
			{
				PROPVARIANT value = *(reinterpret_cast<PROPVARIANT*>(ptrValue->Value));

				if (0 != PropVariantCompare(_FocalPlaneXResolution, value))
				{
					HRESULT hr = PropVariantCopy(&_FocalPlaneXResolution, &value);

					if (SUCCEEDED(hr))
					{
						OnPropertyChanged("PtrFocalPlaneXResolution");
					}
				}
			}
		};

		//PtrFocalPlaneYResolution
		property Platform::IBox<intptr_t>^ PtrFocalPlaneYResolution
		{
			Platform::IBox<intptr_t>^ get()
			{
				return ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&_FocalPlaneYResolution));
			}
			void set(Platform::IBox<intptr_t>^ ptrValue)
			{
				PROPVARIANT value = *(reinterpret_cast<PROPVARIANT*>(ptrValue->Value));

				if (0 != PropVariantCompare(_FocalPlaneYResolution, value))
				{
					HRESULT hr = PropVariantCopy(&_FocalPlaneYResolution, &value);

					if (SUCCEEDED(hr))
					{
						OnPropertyChanged("PtrFocalPlaneYResolution");
					}
				}
			}
		};

		//HasThumbnail
		property bool HasThumbnail
		{
			bool get()
			{
				return _HasThumbnail;
			}
			void set(bool value)
			{
				if (_HasThumbnail != value)
				{
					_HasThumbnail = value;
					OnPropertyChanged("HasThumbnail");
				}
			}
		}

		//TempFilePath
		property Platform::String^ TempFilePath
		{
			Platform::String^ get()
			{
				return Windows::Storage::ApplicationData::Current->TemporaryFolder->Path + "\\" + _UUID.ToString();
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
