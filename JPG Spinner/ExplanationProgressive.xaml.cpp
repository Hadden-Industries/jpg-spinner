//
// ExplanationProgressive.xaml.cpp
// Implementation of the ExplanationProgressive class
//

#include "pch.h"
#include "ExplanationProgressive.xaml.h"

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

ExplanationProgressive::ExplanationProgressive()
{
	InitializeComponent();

	_dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

	_resourceLoader = Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView();

	vector = ref new Platform::Collections::Vector<Windows::UI::Xaml::Media::Imaging::WriteableBitmap^>();
}

void ExplanationProgressive::OnNavigatedTo(NavigationEventArgs^ e)
{
	auto uri = ref new Uri("ms-appx:///assets/progressive.jpg");

	concurrency::create_task(Windows::Storage::StorageFile::GetFileFromApplicationUriAsync(uri))
		.then([this](Windows::Storage::StorageFile^ file)
	{
		return file->OpenAsync(Windows::Storage::FileAccessMode::Read);
	})
		.then([this](Windows::Storage::Streams::IRandomAccessStream^ inputStream)
	{
		Microsoft::WRL::ComPtr<IStream> pIStream;

		HRESULT hr = CreateStreamOverRandomAccessStream(
			reinterpret_cast<IUnknown*>(inputStream),
			IID_PPV_ARGS(&pIStream)
			);

		Microsoft::WRL::ComPtr<IWICImagingFactory> wicFactory;

		hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));
		if (FAILED(hr)) { throw Exception::CreateException(hr); }

		Microsoft::WRL::ComPtr<IWICBitmapDecoder> bitmapDecoder;

		hr = wicFactory->CreateDecoderFromStream(pIStream.Get(), nullptr, WICDecodeOptions::WICDecodeMetadataCacheOnDemand, &bitmapDecoder);
		if (FAILED(hr)) { throw Exception::CreateException(hr); }

		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frameDecode;

		hr = bitmapDecoder->GetFrame(0, &frameDecode);
		if (FAILED(hr)) { throw Exception::CreateException(hr); }

		UINT width, height;
		hr = frameDecode->GetSize(&width, &height);
		if (FAILED(hr)) { throw Exception::CreateException(hr); }

		/*unsigned int colorContextCount = 0U;

		hr = frameDecode->GetColorContexts(0U, NULL, &colorContextCount);
		if (FAILED(hr) || colorContextCount == 0U)
		{
		colorContextCount = 1U;
		hr = S_OK;
		}

		IWICColorContext **contexts = new IWICColorContext*[colorContextCount];

		for (UINT i = 0U; i < colorContextCount; i++)
		{
		if (SUCCEEDED(hr))
		{
		hr = wicFactory->CreateColorContext(&contexts[i]);
		}
		}

		if (SUCCEEDED(hr))
		{
		hr = frameDecode->GetColorContexts(colorContextCount, contexts, &colorContextCount);

		if (FAILED(hr) || colorContextCount == 0U)
		{
		colorContextCount = 1U;
		// A sRGB color space.
		hr = contexts[0]->InitializeFromExifColorSpace(1U);
		}
		}

		Microsoft::WRL::ComPtr<IWICColorTransform> pColorTransform;

		if (SUCCEEDED(hr))
		{
		WICPixelFormatGUID PixelFormat;

		hr = frameDecode->GetPixelFormat(&PixelFormat);

		if (SUCCEEDED(hr))
		{
		hr = wicFactory->CreateColorTransformer(&pColorTransform);
		if (SUCCEEDED(hr))
		{
		Microsoft::WRL::ComPtr<IWICColorContext> pContextDst;

		hr = wicFactory->CreateColorContext(&pContextDst);

		if (SUCCEEDED(hr))
		{
		hr = pContextDst->InitializeFromExifColorSpace(1U);

		if (SUCCEEDED(hr))
		{
		for (UINT i = 0U; i < colorContextCount; ++i)
		{
		hr = pColorTransform->Initialize(frameDecode.Get(), contexts[i], pContextDst.Get(), PixelFormat);
		if (SUCCEEDED(hr))
		{
		break;
		}
		}
		}
		}
		}
		}
		}

		for (UINT i = 0U; i < colorContextCount; i++)
		{
		SafeRelease(&contexts[i]);
		}
		delete[] contexts;

		if (FAILED(hr))
		{
		colorContextCount = 0U;
		hr = S_OK;
		}*/

		Microsoft::WRL::ComPtr<IWICProgressiveLevelControl> pProgressive;

		hr = frameDecode.Get()->QueryInterface(IID_PPV_ARGS(&pProgressive));

		if (SUCCEEDED(hr))
		{
			// Do not use GetLevelCount, it requires all the data to be available up-front

			for (UINT uCurrentLevel = 0; SUCCEEDED(hr); uCurrentLevel++)
			{
				hr = pProgressive->SetCurrentLevel(uCurrentLevel);

				if (WINCODEC_ERR_INVALIDPROGRESSIVELEVEL == hr)
				{
					// No more levels
					TextBlockProgressive->Text = "Progressive level " + currentLevel.ToString() + "/" + (vector->Size - 1U).ToString();

					break;
				}

				if (SUCCEEDED(hr))
				{
					// Output the current level

					Microsoft::WRL::ComPtr<IWICFormatConverter> pConverter;

					if (SUCCEEDED(hr))
					{
						// Convert the image format to 32bppPBGRA
						// (DXGI_FORMAT_B8G8R8A8_UNORM + D2D1_ALPHA_MODE_PREMULTIPLIED).
						hr = wicFactory->CreateFormatConverter(&pConverter);
					}

					if (SUCCEEDED(hr))
					{
						hr = pConverter->Initialize(
							//(colorContextCount == 0U) ? (IWICBitmapSource*)frameDecode.Get() : (IWICBitmapSource*)pColorTransform.Get(),
							frameDecode.Get(),
							GUID_WICPixelFormat32bppPBGRA,
							WICBitmapDitherTypeNone,
							NULL,
							0.0f,
							WICBitmapPaletteTypeMedianCut
							);
					}

					if (FAILED(hr)) { throw Exception::CreateException(hr); }

					auto writeableBitmap = ref new Windows::UI::Xaml::Media::Imaging::WriteableBitmap(width, height);

					Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;

					HRESULT hr = (reinterpret_cast<IUnknown*>(writeableBitmap->PixelBuffer))->QueryInterface<Windows::Storage::Streams::IBufferByteAccess>(&bufferByteAccess);
					if (FAILED(hr)) { throw Exception::CreateException(hr); }

					// Should not be freed; it doesn't belong to us.
					byte* pixelData;

					hr = bufferByteAccess->Buffer(&pixelData);
					if (FAILED(hr)) { throw Exception::CreateException(hr); }

					hr = pConverter->CopyPixels(nullptr, width * 4, width * height * 4, pixelData);
					if (FAILED(hr)) { throw Exception::CreateException(hr); }

					vector->Append(writeableBitmap);
					
					if (0U == uCurrentLevel)
					{
						currentLevel = uCurrentLevel;
						this->DisplayImage->Source = vector->GetAt(currentLevel);
					}
				}
			}
		}

	});
}

void JPG_Spinner::ExplanationProgressive::IncreaseProgressiveLevel(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	UINT increment = 1U;

	switch (vector->Size)
	{
	case 7U:
		increment = 3U;
		break;
	case 10U:
		increment = 3U;
		break;
	default:
		increment = 1U;
	}

	if (currentLevel + increment < vector->Size)
	{
		currentLevel = currentLevel + increment;
	}
	else
	{
		currentLevel = 0U;
	}

	TextBlockProgressive->Text = "Progressive level " + currentLevel.ToString() + "/" + (vector->Size - 1U).ToString();

	this->DisplayImage->Source = vector->GetAt(currentLevel);
}

void JPG_Spinner::ExplanationProgressive::DecreaseProgressiveLevel(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	UINT increment = 1U;

	switch (vector->Size)
	{
	case 7U:
		increment = 3U;
		break;
	case 10U:
		increment = 3U;
		break;
	default:
		increment = 1U;
	}

	if (static_cast<int>(currentLevel) - static_cast<int>(increment) >= 0)
	{
		currentLevel = currentLevel - increment;
	}
	else
	{
		currentLevel = vector->Size - 1U;
	}

	TextBlockProgressive->Text = "Progressive level " + currentLevel.ToString() + "/" + (vector->Size - 1U).ToString();

	this->DisplayImage->Source = vector->GetAt(currentLevel);
}


void JPG_Spinner::ExplanationProgressive::ButtonLeftTextBlock_PointerEntered(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	VisualStateManager::GoToState(ButtonLeft, "PointerOverLeft", false);
}


void JPG_Spinner::ExplanationProgressive::ButtonLeftTextBlock_PointerExited(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	VisualStateManager::GoToState(ButtonLeft, "NormalLeft", false);
}

void JPG_Spinner::ExplanationProgressive::ButtonRightTextBlock_PointerEntered(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	VisualStateManager::GoToState(ButtonRight, "PointerOverRight", false);
}


void JPG_Spinner::ExplanationProgressive::ButtonRightTextBlock_PointerExited(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	VisualStateManager::GoToState(ButtonRight, "NormalRight", false);
}