//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace JPG_Spinner;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

MainPage::MainPage()
{
	InitializeComponent();

	pIWICImagingFactory = nullptr;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (SUCCEEDED(hr))
	{
		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pIWICImagingFactory)
			);
	}
}

MainPage::~MainPage()
{
	if (pIWICImagingFactory)
	{
		pIWICImagingFactory->Release();
	}
}

static HRESULT PROPVAR_ConvertNumber(
	PROPVARIANT *pv,
	int dest_bits,
	BOOL dest_signed,
	LONGLONG *res
	)
{
	BOOL src_signed;

	switch (pv->vt)
	{
	case VT_I1:
		src_signed = TRUE;
		*res = pv->cVal;
		break;
	case VT_UI1:
		src_signed = FALSE;
		*res = pv->bVal;
		break;
	case VT_I2:
		src_signed = TRUE;
		*res = pv->iVal;
		break;
	case VT_UI2:
		src_signed = FALSE;
		*res = pv->uiVal;
		break;
	case VT_I4:
		src_signed = TRUE;
		*res = pv->lVal;
		break;
	case VT_UI4:
		src_signed = FALSE;
		*res = pv->ulVal;
		break;
	case VT_I8:
		src_signed = TRUE;
		*res = pv->hVal.QuadPart;
		break;
	case VT_UI8:
		src_signed = FALSE;
		*res = pv->uhVal.QuadPart;
		break;
	case VT_EMPTY:
		src_signed = FALSE;
		*res = 0;
		break;
	default:
		return E_NOTIMPL;
	}

	if (*res < 0 && src_signed != dest_signed)
		return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);

	if (dest_bits < 64)
	{
		if (dest_signed)
		{
			if (*res >= ((LONGLONG)1 << (dest_bits - 1)) ||
				*res < ((LONGLONG)-1 << (dest_bits - 1)))
				return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
		}
		else
		{
			if ((ULONGLONG)(*res) >= ((ULONGLONG)1 << dest_bits))
				return HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW);
		}
	}

	return S_OK;
}

HRESULT WINAPI PropVariantToUInt16(PROPVARIANT *propvarIn, USHORT *ret)
{
	LONGLONG res;
	HRESULT hr;

	hr = PROPVAR_ConvertNumber(propvarIn, 16, FALSE, &res);
	if (SUCCEEDED(hr)) *ret = (USHORT)res;
	return hr;
}

HRESULT GetJPEGOrientationFlag(IStream * FileStream, USHORT &OrientationFlag, IWICImagingFactory * pIWICImagingFactory)
{
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;
	GUID guidContainerFormat;

	HRESULT hr = pIWICImagingFactory->CreateDecoderFromStream(
		FileStream,
		NULL,
		WICDecodeMetadataCacheOnLoad,
		&pDecoder
		);

	if (FAILED(hr)) // try to load without caching metadata
	{
		hr = pIWICImagingFactory->CreateDecoderFromStream(
			FileStream,
			NULL,
			WICDecodeMetadataCacheOnDemand,
			&pDecoder
			);
	}

	if (SUCCEEDED(hr))
	{
		hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	}

	if (SUCCEEDED(hr))
	{
		if (guidContainerFormat != GUID_ContainerFormatJpeg)
		{
			return E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = pDecoder->GetFrame(0, &pSource);
	}

	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pQueryReader;

		PROPVARIANT propvariantOrientationFlag = { 0 };
		PropVariantInit(&propvariantOrientationFlag);

		hr = pSource->GetMetadataQueryReader(&pQueryReader);

		if (SUCCEEDED(hr))
		{
			hr = pQueryReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &propvariantOrientationFlag);
			// if cannot find EXIF orientation try xmp
			if (FAILED(hr))
			{
				hr = pQueryReader->GetMetadataByName(L"/xmp/tiff:Orientation", &propvariantOrientationFlag);
			}
		}

		if (SUCCEEDED(hr))
		{
			hr = PropVariantToUInt16(&propvariantOrientationFlag, &OrientationFlag);
		}

		PropVariantClear(&propvariantOrientationFlag);
	}

	return hr;
}

HRESULT SetJPEGOrientationFlag(IStream * FileStream, const USHORT OrientationFlag, IWICImagingFactory * pIWICImagingFactory)
{
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;
	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;
	GUID guidContainerFormat;

	HRESULT hr = pIWICImagingFactory->CreateDecoderFromStream(
		FileStream,
		NULL,
		WICDecodeMetadataCacheOnDemand, // A decoder must be created using the WICDecodeOptions value WICDecodeMetadataCacheOnDemand to perform in-place metadata updates
		&pDecoder
		);

	if (SUCCEEDED(hr))
	{
		hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	}

	if (SUCCEEDED(hr))
	{
		if (guidContainerFormat != GUID_ContainerFormatJpeg)
		{
			return E_FAIL;
		}
	}

	if (SUCCEEDED(hr))
	{
		hr = pDecoder->GetFrame(0, &pSource);
	}

	if (SUCCEEDED(hr))
	{
		Microsoft::WRL::ComPtr<IWICFastMetadataEncoder> pFME;
		Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> pFMEQW;

		hr = pIWICImagingFactory->CreateFastMetadataEncoderFromFrameDecode(pSource.Get(), &pFME);

		if (SUCCEEDED(hr))
		{
			hr = pFME->GetMetadataQueryWriter(&pFMEQW);
		}

		if (SUCCEEDED(hr))
		{
			Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pQueryReader;
			PROPVARIANT propvariantOrientationFlag = { 0 };
			PROPVARIANT value = { 0 };

			PropVariantInit(&propvariantOrientationFlag);
			PropVariantInit(&value);

			value.vt = VT_UI2;
			value.uiVal = OrientationFlag;

			hr = pSource->GetMetadataQueryReader(&pQueryReader);

			if (SUCCEEDED(hr))
			{
				HRESULT hrEXIF = S_OK;

				hr = pQueryReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &propvariantOrientationFlag);

				if (SUCCEEDED(hr) && propvariantOrientationFlag.vt != VT_EMPTY)
				{
					hr = pFMEQW->SetMetadataByName(L"/app1/ifd/{ushort=274}", &value);
				}

				hrEXIF = hr;

				PropVariantClear(&propvariantOrientationFlag);

				hr = pQueryReader->GetMetadataByName(L"/xmp/tiff:Orientation", &propvariantOrientationFlag);

				// usually WINCODEC_ERR_PROPERTYNOTFOUND == hr here
				if (SUCCEEDED(hr) && propvariantOrientationFlag.vt != VT_EMPTY)
				{
					hr = pFMEQW->SetMetadataByName(L"/xmp/tiff:Orientation", &value);
				}
				// if the XMP update failed, then 
				else
				{
					hr = hrEXIF;
				}
			}

			PropVariantClear(&propvariantOrientationFlag);
			PropVariantClear(&value);
		}

		if (SUCCEEDED(hr))
		{
			hr = pFME->Commit();
		}
	}

	return hr;
}

void JPG_Spinner::MainPage::Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	auto openPicker = ref new Windows::Storage::Pickers::FileOpenPicker();
	openPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::PicturesLibrary;
	openPicker->ViewMode = Windows::Storage::Pickers::PickerViewMode::Thumbnail;

	// Filter to include a sample subset of file types.
	openPicker->FileTypeFilter->Clear();
	openPicker->FileTypeFilter->Append(".jpg");
	openPicker->FileTypeFilter->Append(".jpeg");

	// All this work will be done asynchronously on a background thread:

	// Wrap the async call inside a concurrency::task object
	concurrency::create_task(openPicker->PickSingleFileAsync())

	// Accept the unwrapped return value of previous call as input param
	.then([this](Windows::Storage::StorageFile^ file)
	{
		// file is null if user cancels the file picker.
		if (file == nullptr)
		{
			// Stop work and clean up.
			concurrency::cancel_current_task();
		}

		// For data binding text blocks to file properties
		this->DataContext = file;

		// Add picked file to MostRecentlyUsedList.
		mruToken = Windows::Storage::AccessCache::StorageApplicationPermissions::MostRecentlyUsedList->Add(file);
		
		// Return the IRandomAccessStream^ object
		return file->OpenAsync(Windows::Storage::FileAccessMode::Read);

	})
	.then([this](Windows::Storage::Streams::IRandomAccessStream^ fileStream)
	{
		Microsoft::WRL::ComPtr<IStream> pIStream;

		auto hr = CreateStreamOverRandomAccessStream(
			reinterpret_cast<IUnknown*>(fileStream),
			IID_PPV_ARGS(&pIStream)
			);

		if (SUCCEEDED(hr))
		{
			USHORT OrientationFlagValue = 0U;

			hr = GetJPEGOrientationFlag(pIStream.Get(), OrientationFlagValue, pIWICImagingFactory);

			if (SUCCEEDED(hr))
			{
				OrientationFlag->Text = "Orientation flag: " + OrientationFlagValue;
			}
			else
			{
				OrientationFlag->Text = "No orientation flag in metadata";
			}
		}
		// Set the stream as source of the bitmap
		Windows::UI::Xaml::Media::Imaging::BitmapImage^ bitmapImage = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage();
		bitmapImage->SetSource(fileStream);

		// Set the bitmap as source of the Image control
		displayImage->Source = bitmapImage;
	});
}
