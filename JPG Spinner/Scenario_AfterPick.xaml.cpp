//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// Scenario_AfterPick.xaml.cpp
// Implementation of the Scenario_AfterPick class
//

#include "pch.h"
#include "Scenario_AfterPick.xaml.h"
#include "ItemViewer.xaml.h"

using namespace JPG_Spinner;

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;

HRESULT CreateTempFile(Platform::String^ filePath)
{
	HANDLE hTempFile = INVALID_HANDLE_VALUE;

	CREATEFILE2_EXTENDED_PARAMETERS extendedParams = { 0 };
	extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
	extendedParams.dwFileAttributes = FILE_ATTRIBUTE_TEMPORARY;
	extendedParams.dwFileFlags = FILE_FLAG_RANDOM_ACCESS;
	extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
	extendedParams.lpSecurityAttributes = nullptr;
	extendedParams.hTemplateFile = nullptr;

	hTempFile = CreateFile2(
		filePath->Data(),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		CREATE_ALWAYS,
		&extendedParams);

	if (INVALID_HANDLE_VALUE == hTempFile)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	if (FALSE == CloseHandle(hTempFile))
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	return S_OK;
}

FILE* CreateTempFile(const wchar_t* filePath)
{
	HANDLE hTempFile = INVALID_HANDLE_VALUE;

	CREATEFILE2_EXTENDED_PARAMETERS extendedParams = { 0 };
	extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
	extendedParams.dwFileAttributes = FILE_ATTRIBUTE_TEMPORARY;
	extendedParams.dwFileFlags = FILE_FLAG_RANDOM_ACCESS;
	extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
	extendedParams.lpSecurityAttributes = nullptr;
	extendedParams.hTemplateFile = nullptr;

	hTempFile = CreateFile2(
		filePath,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		CREATE_ALWAYS,
		&extendedParams);

	if (INVALID_HANDLE_VALUE == hTempFile)
	{
		return nullptr;
	}

	// Open the output file from the handle
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hTempFile), _O_RDWR | _O_BINARY);

	if (-1 == fd)
	{
		CloseHandle(hTempFile);

		SetLastError(ERROR_CANNOT_MAKE);

		return nullptr;
	}

	FILE * fp = _fdopen(fd, "w+b");

	if (0 == fp)
	{
		// Also calls CloseHandle()
		_close(fd);

		SetLastError(ERROR_CANNOT_MAKE);

		return nullptr;
	}

	return fp;
}

concurrency::task<HRESULT> GetMetadataAsync(Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	return concurrency::create_task(item->StorageFile->OpenAsync(Windows::Storage::FileAccessMode::Read))
		.then([item, pIWICImagingFactory](Windows::Storage::Streams::IRandomAccessStream^ fileStream)
	{
		Microsoft::WRL::ComPtr<IStream> pIStream;

		HRESULT hr = CreateStreamOverRandomAccessStream(reinterpret_cast<IUnknown*>(fileStream), IID_PPV_ARGS(&pIStream));
		if (FAILED(hr)) { return hr; }

		Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;

		hr = pIWICImagingFactory->CreateDecoderFromStream(
			pIStream.Get(),
			NULL,
			WICDecodeMetadataCacheOnLoad,
			&pDecoder
			);

		if (FAILED(hr))
		{
			// Try to load without caching metadata
			hr = pIWICImagingFactory->CreateDecoderFromStream(
				pIStream.Get(),
				NULL,
				WICDecodeMetadataCacheOnDemand,
				&pDecoder
				);
		}
		if (FAILED(hr)) { return hr; }

		GUID guidContainerFormat = GUID_NULL;

		hr = pDecoder->GetContainerFormat(&guidContainerFormat);
		if (FAILED(hr)) { return hr; }

		if (GUID_ContainerFormatJpeg != guidContainerFormat)
		{
			return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
		}

		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;

		hr = pDecoder->GetFrame(0U, &pSource);
		if (FAILED(hr)) { return hr; }

		Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pQueryReader;

		hr = pSource->GetMetadataQueryReader(&pQueryReader);
		if (FAILED(hr)) { return hr; }

		PROPVARIANT propVariant = { 0 };
		PropVariantInit(&propVariant);

		HRESULT hrLastSuccessful = hr;

		// Orientation
		hr = pQueryReader->GetMetadataByName(L"/app1/ifd/{ushort=274}", &propVariant);

		if (SUCCEEDED(hr))
		{
			item->Orientation = static_cast<unsigned char>(propVariant.uiVal);
		}
		else
		{
			hr = hrLastSuccessful;
		}

		// Clear value for new query
		PropVariantClear(&propVariant);

		hrLastSuccessful = hr;

		// XMP Orientation
		hr = pQueryReader->GetMetadataByName(L"/xmp/tiff:Orientation", &propVariant);

		if (SUCCEEDED(hr))
		{
			item->OrientationXMP = static_cast<unsigned char>(propVariant.uiVal);
		}
		else
		{
			hr = hrLastSuccessful;
		}

		PropVariantClear(&propVariant);

		// return early if the orientation value is unusable
		if (!(item->OrientationCoalesced >= 2U && item->OrientationCoalesced <= 8U))
		{
			return S_OK;
		}

		hrLastSuccessful = hr;

		// Thumbnail
		Microsoft::WRL::ComPtr<IWICBitmapSource> pIWICBitmapSource;

		hr = pSource->GetThumbnail(&pIWICBitmapSource);

		if (SUCCEEDED(hr) && nullptr != pIWICBitmapSource)
		{
			item->HasThumbnail = true;
		}
		else
		{
			hr = hrLastSuccessful;
		}

		/*hrLastSuccessful = hr;

		// Note the details of the thumbnail, if any
		hr = pQueryReader->GetMetadataByName(L"/app1/thumb/{ushort=259}", &propVariant);

		// Compression must be 6 for the thumbnail to have the the JPEG Interchange values
		if (SUCCEEDED(hr) && 6U == propVariant.uiVal)
		{
			PropVariantClear(&propVariant);

			hrLastSuccessful = hr;

			hr = pQueryReader->GetMetadataByName(L"/app1/thumb/{ushort=513}", &propVariant);

			if (SUCCEEDED(hr))
			{
				item->JPEGInterchangeFormat = propVariant.uiVal;
			}
			else
			{
				hr = hrLastSuccessful;
			}

			PropVariantClear(&propVariant);

			hrLastSuccessful = hr;

			hr = pQueryReader->GetMetadataByName(L"/app1/thumb/{ushort=514}", &propVariant);

			if (SUCCEEDED(hr))
			{
				item->JPEGInterchangeFormatLength = propVariant.uiVal;
			}
			else
			{
				hr = hrLastSuccessful;
			}
		}
		else
		{
			hr = hrLastSuccessful;
		}

		PropVariantClear(&propVariant);

		hrLastSuccessful = hr;*/

		//SubjectArea
		hr = pQueryReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=37396}", &propVariant);

		if (SUCCEEDED(hr))
		{
			item->PtrSubjectArea = ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&propVariant));
		}
		else
		{
			hr = hrLastSuccessful;
		}

		PropVariantClear(&propVariant);

		hrLastSuccessful = hr;

		//SubjectLocation
		hr = pQueryReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=41492}", &propVariant);
		if (SUCCEEDED(hr))
		{
			item->PtrSubjectLocation = ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&propVariant));
		}
		else
		{
			hr = hrLastSuccessful;
		}

		PropVariantClear(&propVariant);

		hrLastSuccessful = hr;

		//FocalPlaneXResolution
		hr = pQueryReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=41486}", &propVariant);
		if (SUCCEEDED(hr))
		{
			item->PtrFocalPlaneXResolution = ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&propVariant));
		}
		else
		{
			hr = hrLastSuccessful;
		}

		PropVariantClear(&propVariant);

		hrLastSuccessful = hr;

		//FocalPlaneYResolution
		hr = pQueryReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=41487}", &propVariant);
		if (SUCCEEDED(hr))
		{
			item->PtrFocalPlaneYResolution = ref new Platform::Box<intptr_t>(reinterpret_cast<intptr_t>(&propVariant));
		}
		else
		{
			hr = hrLastSuccessful;
		}

		PropVariantClear(&propVariant);

		//ThumbnailData
		//hr = pQueryReader->GetMetadataByName(L"/app0/{ushort=6}", &propvariantOrientationFlag);
		//WINCODEC_ERR_PROPERTYNOTFOUND

		return hr;
	});
}

/*// Zero the thumbnail App1/1st IFD block
HRESULT DeleteJPEGThumbnailMetadata(Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;

	HRESULT hr = pIWICImagingFactory->CreateDecoderFromFilename(
		item->TempFilePath->Data(),
		NULL,
		GENERIC_READ | GENERIC_WRITE,
		WICDecodeMetadataCacheOnDemand, // A decoder must be created using the WICDecodeOptions value WICDecodeMetadataCacheOnDemand to perform in-place metadata updates
		&pDecoder);
	if (FAILED(hr)) { return hr; }

	GUID guidContainerFormat = GUID_NULL;

	hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	if (FAILED(hr)) { return hr; }

	if (guidContainerFormat != GUID_ContainerFormatJpeg)
	{
		return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;

	hr = pDecoder->GetFrame(0, &pSource);
	if (FAILED(hr)) { return hr; }

	// Set the orientation value to normal
	Microsoft::WRL::ComPtr<IWICFastMetadataEncoder> pFME;

	hr = pIWICImagingFactory->CreateFastMetadataEncoderFromFrameDecode(pSource.Get(), &pFME);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> pFMEQW;

	hr = pFME->GetMetadataQueryWriter(&pFMEQW);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pQueryReader;

	hr = pSource->GetMetadataQueryReader(&pQueryReader);
	if (FAILED(hr)) { return hr; }

	Platform::String^ pathMetadata = "/app1/thumb";

	PROPVARIANT value = { 0 };
	PropVariantInit(&value);

	hr = pQueryReader->GetMetadataByName(pathMetadata->Data(), &value);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pEmbedReader;

	hr = value.punkVal->QueryInterface(IID_PPV_ARGS(&pEmbedReader));
	if (FAILED(hr)) { return hr; }

	PropVariantClear(&value);

	Microsoft::WRL::ComPtr<IEnumString> metadataItems;

	hr = pEmbedReader->GetEnumerator(&metadataItems);

	// Enumerate through all values, commonly those below
	// Compression /app1/thumb/{ushort=259}
	// XResolution /app1/thumb/{ushort=282}
	// YResolution /app1/thumb/{ushort=283}
	// ResolutionUnit /app1/thumb/{ushort=296}
	// /app1/thumb/{} this deletes the two below
	// JPEGInterchangeFormat /app1/thumb/{ushort=513}
	// JPEGInterchangeFormatLength /app1/thumb/{ushort=514}
	for (; SUCCEEDED(hr);)
	{
		ULONG metadataItemsFetched = 0U;

		LPOLESTR metadataItem;

		hr = metadataItems->Next(1U, &metadataItem, &metadataItemsFetched);

		// Next returns S_FALSE when there are no more items, so check metadataItemsFetched
		if (SUCCEEDED(hr) && 1U == metadataItemsFetched)
		{
			hr = pFMEQW->RemoveMetadataByName((pathMetadata + ref new Platform::String(metadataItem))->Data());

			// metadataItem is allocated by Next and needs to be freed
			CoTaskMemFree(metadataItem);
		}
		else
		{
			break;
		}
	}

	return hr;
}

HRESULT DeleteJPEGThumbnailData(Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	// Note that the thumbnail offset is at JPEGInterchangeFormat + 12U from the beginning of the file (may not be 12 when JFIF...)
	// Currently trim off the padding, NOT the thumbnail data! Need to use the offset value

	if (Platform::Guid(GUID_NULL) == item->UUID || 0U == item->JPEGInterchangeFormat || 0U == item->JPEGInterchangeFormatLength)
	{
		return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
	}

	HRESULT hr = DeleteJPEGThumbnailMetadata(item, pIWICImagingFactory);
	if (FAILED(hr)) { return hr; }

	Platform::String^ filePathSuffix = ".temp";

	FILE * tempFilePointer = CreateTempFile((item->TempFilePath + filePathSuffix)->Data());

	if (nullptr == tempFilePointer)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	HANDLE hTempFile = INVALID_HANDLE_VALUE;

	CREATEFILE2_EXTENDED_PARAMETERS extendedParams = { 0 };
	extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
	extendedParams.dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	extendedParams.dwFileFlags = FILE_FLAG_SEQUENTIAL_SCAN;
	extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
	extendedParams.lpSecurityAttributes = nullptr;
	extendedParams.hTemplateFile = nullptr;

	hTempFile = CreateFile2(
		item->TempFilePath->Data(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		OPEN_EXISTING,
		&extendedParams
		);

	if (INVALID_HANDLE_VALUE == hTempFile)
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	// Open the output file from the handle
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hTempFile), _O_RDONLY | _O_BINARY);

	if (-1 == fd)
	{
		CloseHandle(hTempFile);

		return HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE);
	}

	FILE * filePointer = _fdopen(fd, "rb");

	if (0 == filePointer)
	{
		// Also calls CloseHandle()
		_close(fd);

		return HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE);
	}

	byte fileHead[0xFFFF] = { 0 };

	// Read File head
	if (6 != fread(fileHead, sizeof(byte), 6, filePointer))
	{
		return HRESULT_FROM_WIN32(ERROR_READ_FAULT);
	}

	// Check for JPEG SOI + Exif APP1
	if (fileHead[0] != 0xFF ||
		fileHead[1] != 0xD8 ||
		fileHead[2] != 0xFF ||
		fileHead[3] != 0xE1)
	{
		return WINCODEC_ERR_BADHEADER;
	}

	// Get App1 length (all 2-byte quantities in JPEG markers are MSB first)
	unsigned short lengthApp1 = ((static_cast<unsigned short>(fileHead[4])) << 8) + (static_cast<unsigned short>(fileHead[5]));

	// Sanity check that the thumbnail can fit within the App1 block
	if (item->JPEGInterchangeFormatLength >= lengthApp1)
	{
		return WINCODEC_ERR_BADMETADATAHEADER;
	}

	lengthApp1 = lengthApp1 - item->JPEGInterchangeFormatLength;

	// Write the new length in
	fileHead[4] = (lengthApp1 >> 8) & 0xFF;
	fileHead[5] = lengthApp1 & 0xFF;

	// Read the remainder of the App1 block
	if (lengthApp1 != fread(&fileHead[6], sizeof(byte), lengthApp1, filePointer))
	{
		return HRESULT_FROM_WIN32(ERROR_READ_FAULT);
	}

	// Subtract 2 bytes for the length of the length field itself
	if (static_cast<size_t>(6U + lengthApp1 - 2U) != fwrite(fileHead, sizeof(byte), static_cast<size_t>(6U + lengthApp1 - 2U), tempFilePointer))
	{
		return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
	}

	// Skip the thumbnail data block
	if (0 == fseek(filePointer, item->JPEGInterchangeFormatLength - 2U, SEEK_CUR))
	{
		int c = 0;

		// Read one char until End Of File
		while ((c = getc(filePointer)) != EOF)
		{
			// Put the char into the new file
			if (EOF == putc(c, tempFilePointer))
			{
				return HRESULT_FROM_WIN32(ERROR_WRITE_FAULT);
			}
		}
	}
	else
	{
		return HRESULT_FROM_WIN32(ERROR_READ_FAULT);
	}

	// Close output file, also calls _close()
	if (0 == fclose(filePointer))
	{
		if (0 == fclose(tempFilePointer))
		{
			if (0 == MoveFileExW(
				(item->TempFilePath + filePathSuffix)->Data(),
				item->TempFilePath->Data(),
				MOVEFILE_REPLACE_EXISTING))
			{
				return HRESULT_FROM_WIN32(GetLastError());
			}
		}
	}

	return S_OK;
}*/

HRESULT FixMetadataOutOfPlace(Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	// Set up the orientation helper
	auto orientationHelper = new OrientationHelper(item->OrientationCoalesced);

	if (VT_EMPTY == (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->vt &&
		VT_EMPTY == (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectLocation->Value))->vt &&
		(
			(
				VT_EMPTY == (reinterpret_cast<PROPVARIANT*>(item->PtrFocalPlaneXResolution->Value))->vt &&
				VT_EMPTY == (reinterpret_cast<PROPVARIANT*>(item->PtrFocalPlaneYResolution->Value))->vt
			)
			||
			(
				!orientationHelper->XYFlips()
			)
		) &&
		!item->HasThumbnail)
	{
		// Nothing to fix
		return S_OK;
	}

	Platform::String^ filePathSuffix = ".mdop";

	HRESULT hr = CreateTempFile(item->TempFilePath + filePathSuffix);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICBitmapDecoder> piDecoder;

	// Create the decoder.
	hr = pIWICImagingFactory->CreateDecoderFromFilename(
		item->TempFilePath->Data(),
		NULL,
		GENERIC_READ,
		WICDecodeMetadataCacheOnDemand, //For JPEG lossless decoding/encoding.
		&piDecoder);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICStream> piFileStream;

	// Create a file stream.
	hr = pIWICImagingFactory->CreateStream(&piFileStream);
	if (FAILED(hr)) { return hr; }

	// Initialize our new file stream.
	hr = piFileStream->InitializeFromFilename((item->TempFilePath + filePathSuffix)->Data(), GENERIC_WRITE);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICBitmapEncoder> piEncoder;

	// Create the encoder.
	hr = pIWICImagingFactory->CreateEncoder(GUID_ContainerFormatJpeg, NULL, &piEncoder);
	if (FAILED(hr)) { return hr; }

	// Initialize the encoder
	hr = piEncoder->Initialize(piFileStream.Get(), WICBitmapEncoderNoCache);
	if (FAILED(hr)) { return hr; }

	UINT count = 0U;

	hr = piDecoder->GetFrameCount(&count);
	if (FAILED(hr)) { return hr; }

	// Process each frame of the image.
	for (UINT i = 0; i < count; i++)
	{
		// Frame variables.
		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> piFrameDecode;
		Microsoft::WRL::ComPtr<IWICBitmapFrameEncode> piFrameEncode;
		Microsoft::WRL::ComPtr<IWICMetadataQueryReader> piFrameQReader;
		Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> piFrameQWriter;

		// Get and create the image frame.
		hr = piDecoder->GetFrame(i, &piFrameDecode);
		if (FAILED(hr)) { return hr; }

		hr = piEncoder->CreateNewFrame(&piFrameEncode, NULL);
		if (FAILED(hr)) { return hr; }

		// Initialize the encoder.
		hr = piFrameEncode->Initialize(NULL);
		if (FAILED(hr)) { return hr; }

		UINT width, height = 0U;

		// Get and set the size.
		hr = piFrameDecode->GetSize(&width, &height);
		if (FAILED(hr)) { return hr; }

		hr = piFrameEncode->SetSize(width, height);
		if (FAILED(hr)) { return hr; }

		double dpiX, dpiY = 0.0;

		// Get and set the resolution.
		piFrameDecode->GetResolution(&dpiX, &dpiY);
		if (FAILED(hr)) { return hr; }

		hr = piFrameEncode->SetResolution(dpiX, dpiY);
		if (FAILED(hr)) { return hr; }

		WICPixelFormatGUID pixelFormat = GUID_NULL;

		// Set the pixel format.
		piFrameDecode->GetPixelFormat(&pixelFormat);
		if (FAILED(hr)) { return hr; }

		hr = piFrameEncode->SetPixelFormat(&pixelFormat);
		if (FAILED(hr)) { return hr; }

		Microsoft::WRL::ComPtr<IWICMetadataBlockReader> piBlockReader;

		// Copy metadata using metadata block reader/writer.
		hr = piFrameDecode.As(&piBlockReader);
		if (FAILED(hr)) { return hr; }

		Microsoft::WRL::ComPtr<IWICMetadataBlockWriter> piBlockWriter;

		hr = piFrameEncode.As(&piBlockWriter);
		if (FAILED(hr)) { return hr; }

		hr = piBlockWriter->InitializeFromBlockReader(piBlockReader.Get());
		if (FAILED(hr)) { return hr; }

		hr = piFrameEncode->GetMetadataQueryWriter(&piFrameQWriter);
		if (FAILED(hr)) { return hr; }

		// Set the SubjectArea value according to the re-oriented coordinates
		if (VT_EMPTY != (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->vt)
		{
			PROPVARIANT propvariantToSet = { 0 };
			PropVariantInit(&propvariantToSet);

			hr = PropVariantCopy(&propvariantToSet, reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value));
			if (FAILED(hr)) { return hr; }

			auto coordinate = Windows::Foundation::Point(static_cast<float>((reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->caui.pElems[0]), static_cast<float>((reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->caui.pElems[1]));

			auto matrix = orientationHelper->GetMatrix();

			if (orientationHelper->XYFlips())
			{
				coordinate.X = coordinate.X - (static_cast<float>(height) / static_cast<float>(2.0));
				coordinate.Y = coordinate.Y - (static_cast<float>(width) / static_cast<float>(2.0));
			}
			else
			{
				coordinate.X = coordinate.X - (static_cast<float>(width) / static_cast<float>(2.0));
				coordinate.Y = coordinate.Y - (static_cast<float>(height) / static_cast<float>(2.0));
			}

			coordinate = matrix.Transform(coordinate);

			coordinate.X = coordinate.X + (static_cast<float>(width) / static_cast<float>(2.0));
			coordinate.Y = coordinate.Y + (static_cast<float>(height) / static_cast<float>(2.0));

			propvariantToSet.caui.pElems[0] = static_cast<USHORT>(coordinate.X);
			propvariantToSet.caui.pElems[1] = static_cast<USHORT>(coordinate.Y);

			if (4U == (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->caui.cElems)
			{
				if (orientationHelper->XYFlips())
				{
					propvariantToSet.caui.pElems[2] = (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->caui.pElems[3];
					propvariantToSet.caui.pElems[3] = (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectArea->Value))->caui.pElems[2];
				}
			}

			hr = piFrameQWriter->SetMetadataByName(L"/app1/ifd/exif/{ushort=37396}", &propvariantToSet);
			if (FAILED(hr)) { return hr; }

			PropVariantClear(&propvariantToSet);
		}

		// Set the SubjectLocation value according to the re-oriented coordinates
		if (VT_EMPTY != (reinterpret_cast<PROPVARIANT*>(item->PtrSubjectLocation->Value))->vt)
		{
			PROPVARIANT propvariantToSet = { 0 };
			PropVariantInit(&propvariantToSet);

			hr = PropVariantCopy(&propvariantToSet, reinterpret_cast<PROPVARIANT*>(item->PtrSubjectLocation->Value));
			if (FAILED(hr)) { return hr; }

			auto coordinate = Windows::Foundation::Point(static_cast<float>((reinterpret_cast<PROPVARIANT*>(item->PtrSubjectLocation->Value))->caui.pElems[0]), static_cast<float>((reinterpret_cast<PROPVARIANT*>(item->PtrSubjectLocation->Value))->caui.pElems[1]));

			auto matrix = orientationHelper->GetMatrix();

			if (orientationHelper->XYFlips())
			{
				coordinate.X = coordinate.X - (static_cast<float>(height) / static_cast<float>(2.0));
				coordinate.Y = coordinate.Y - (static_cast<float>(width) / static_cast<float>(2.0));
			}
			else
			{
				coordinate.X = coordinate.X - (static_cast<float>(width) / static_cast<float>(2.0));
				coordinate.Y = coordinate.Y - (static_cast<float>(height) / static_cast<float>(2.0));
			}

			coordinate = matrix.Transform(coordinate);

			coordinate.X = coordinate.X + (static_cast<float>(width) / static_cast<float>(2.0));
			coordinate.Y = coordinate.Y + (static_cast<float>(height) / static_cast<float>(2.0));

			propvariantToSet.caui.pElems[0] = static_cast<USHORT>(coordinate.X);
			propvariantToSet.caui.pElems[1] = static_cast<USHORT>(coordinate.Y);

			hr = piFrameQWriter->SetMetadataByName(L"/app1/ifd/exif/{ushort=41492}", &propvariantToSet);
			if (FAILED(hr)) { return hr; }

			PropVariantClear(&propvariantToSet);
		}

		if (VT_EMPTY != (reinterpret_cast<PROPVARIANT*>(item->PtrFocalPlaneXResolution->Value))->vt ||
			VT_EMPTY != (reinterpret_cast<PROPVARIANT*>(item->PtrFocalPlaneYResolution->Value))->vt)
		{
			if (orientationHelper->XYFlips())
			{
				// FocalPlaneXResolution
				hr = piFrameQWriter->SetMetadataByName(L"/app1/ifd/exif/{ushort=41486}", reinterpret_cast<PROPVARIANT*>(item->PtrFocalPlaneYResolution->Value));
				if (FAILED(hr)) { return hr; }

				// FocalPlaneYResolution
				hr = piFrameQWriter->SetMetadataByName(L"/app1/ifd/exif/{ushort=41487}", reinterpret_cast<PROPVARIANT*>(item->PtrFocalPlaneXResolution->Value));
				if (FAILED(hr)) { return hr; }
			}
		}

		// If there is a thumbnail
		if (item->HasThumbnail)
		{
			// Rotate it
			Microsoft::WRL::ComPtr<IWICBitmapSource> pIWICBitmapSource;

			hr = piFrameDecode->GetThumbnail(&pIWICBitmapSource);
			if (FAILED(hr)) { return hr; }

			Microsoft::WRL::ComPtr<IWICBitmap> pIWICBitmap;

			// To improve the performance of pIWICBitmapFlipRotator, cache the data
			hr = pIWICImagingFactory->CreateBitmapFromSource(pIWICBitmapSource.Get(), WICBitmapCacheOnLoad, &pIWICBitmap);
			if (FAILED(hr)) { return hr; }

			Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> pIWICBitmapFlipRotator;

			hr = pIWICImagingFactory->CreateBitmapFlipRotator(&pIWICBitmapFlipRotator);
			if (FAILED(hr)) { return hr; }

			hr = pIWICBitmapFlipRotator->Initialize(pIWICBitmap.Get(), orientationHelper->GetWICBitmapTransformOptions());
			if (FAILED(hr)) { return hr; }

			// The source image will be re-encoded as either an 8bpp or 24bpp JPEG and will be written to the JPEG’s APP1 metadata block
			hr = piFrameEncode->SetThumbnail(pIWICBitmapFlipRotator.Get());
			if (FAILED(hr)) { return hr; }
		}

		hr = piFrameEncode->WriteSource(
			static_cast<IWICBitmapSource*>(piFrameDecode.Get()),
			NULL); // Using NULL enables JPEG loss-less encoding.
		if (FAILED(hr)) { return hr; }

		// Commit the frame.
		hr = piFrameEncode->Commit();
		if (FAILED(hr)) { return hr; }
	}

	hr = piEncoder->Commit();
	if (FAILED(hr)) { return hr; }

	// This fails with E_NOTIMPL, and is safe to ignore as per https://msdn.microsoft.com/en-us/library/windows/desktop/aa380036(v=vs.85).aspx
	piFileStream->Commit(STGC_DEFAULT);

	// Release all handles to the files so that MoveFileEx can work
	SafeRelease(piEncoder.GetAddressOf());
	SafeRelease(piFileStream.GetAddressOf());
	SafeRelease(piDecoder.GetAddressOf());

	if (0 == MoveFileExW(
		(item->TempFilePath + filePathSuffix)->Data(),
		item->TempFilePath->Data(),
		MOVEFILE_REPLACE_EXISTING))
	{
		return HRESULT_FROM_WIN32(GetLastError());
	}

	return hr;
}

HRESULT FixMetadata(Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;

	HRESULT hr = pIWICImagingFactory->CreateDecoderFromFilename(
		item->TempFilePath->Data(),
		NULL,
		GENERIC_READ | GENERIC_WRITE,
		WICDecodeMetadataCacheOnDemand, // A decoder must be created using the WICDecodeOptions value WICDecodeMetadataCacheOnDemand to perform in-place metadata updates
		&pDecoder);
	if (FAILED(hr)) { return hr; }

	GUID guidContainerFormat = GUID_NULL;

	hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	if (FAILED(hr)) { return hr; }

	if (guidContainerFormat != GUID_ContainerFormatJpeg)
	{
		return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;

	hr = pDecoder->GetFrame(0, &pSource);
	if (FAILED(hr)) { return hr; }

	// Set the orientation value to normal
	Microsoft::WRL::ComPtr<IWICFastMetadataEncoder> pFME;

	hr = pIWICImagingFactory->CreateFastMetadataEncoderFromFrameDecode(pSource.Get(), &pFME);
	if (FAILED(hr)) { return hr; }

	Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> pFMEQW;

	hr = pFME->GetMetadataQueryWriter(&pFMEQW);
	if (FAILED(hr)) { return hr; }

	PROPVARIANT propvariantOrientationFlag = { 0 };
	PropVariantInit(&propvariantOrientationFlag);
	
	propvariantOrientationFlag.vt = VT_UI2;
	propvariantOrientationFlag.uiVal = 1U;

	if (0U != item->Orientation)
	{
		hr = pFMEQW->SetMetadataByName(L"/app1/ifd/{ushort=274}", &propvariantOrientationFlag);
		if (FAILED(hr)) { return hr; }
	}

	if (0U != item->OrientationXMP)
	{
		hr = pFMEQW->SetMetadataByName(L"/xmp/tiff:Orientation", &propvariantOrientationFlag);
		if (FAILED(hr)) { return hr; }
	}

	PropVariantClear(&propvariantOrientationFlag);

	hr = pFME->Commit();

	return hr;
}

Platform::Guid GetUUID()
{
	GUID result;

	HRESULT hr = CoCreateGuid(&result);
	if (FAILED(hr)) throw Platform::Exception::CreateException(hr);

	return Platform::Guid(result);
}

concurrency::task<FILE *> StorageFileToFilePointerAsync(Windows::Storage::StorageFile^ storageFile)
{
	return concurrency::create_task(Windows::Storage::FileIO::ReadBufferAsync(storageFile))
		.then([](Windows::Storage::Streams::IBuffer^ buffer)
	{
		FILE * filePointer = nullptr;

		Windows::Storage::StorageFolder^ tempFolder = Windows::Storage::ApplicationData::Current->TemporaryFolder;

		if (nullptr != tempFolder)
		{
			HRESULT hr = S_OK;

			Platform::String^ tempFileName = tempFolder->Path + "\\" + GetUUID().ToString();

			CREATEFILE2_EXTENDED_PARAMETERS extendedParams = { 0 };
			extendedParams.dwSize = sizeof(CREATEFILE2_EXTENDED_PARAMETERS);
			extendedParams.dwFileAttributes = FILE_ATTRIBUTE_TEMPORARY;
			extendedParams.dwFileFlags = FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_RANDOM_ACCESS;
			extendedParams.dwSecurityQosFlags = SECURITY_ANONYMOUS;
			extendedParams.lpSecurityAttributes = nullptr;
			extendedParams.hTemplateFile = nullptr;

			HANDLE tempFileHandle = CreateFile2(
				tempFileName->Data(),
				GENERIC_READ | GENERIC_WRITE | DELETE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				CREATE_ALWAYS,
				&extendedParams
				);

			if (INVALID_HANDLE_VALUE != tempFileHandle)
			{
				// Open the output file from the handle
				int fd = _open_osfhandle(reinterpret_cast<intptr_t>(tempFileHandle), _O_RDWR | _O_BINARY);

				if (-1 != fd)
				{
					_set_errno(0);

					filePointer = _fdopen(fd, "w+b");

					if (nullptr != filePointer)
					{
						Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;

						// Query the IBufferByteAccess interface.
						hr = reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));

						if (SUCCEEDED(hr))
						{
							byte* bytes = nullptr;

							// Retrieve the buffer data.
							hr = bufferByteAccess->Buffer(&bytes);

							if (SUCCEEDED(hr))
							{
								if (static_cast<size_t>(buffer->Length) == fwrite(bytes, sizeof(byte), static_cast<size_t>(buffer->Length), filePointer))
								{
									if (0 == fseek(filePointer, 0L, SEEK_SET))
									{
										return filePointer;
									}
								}
							}
						}

						/*// Without COM casts
						Windows::Storage::Streams::DataReader^ reader = Windows::Storage::Streams::DataReader::FromBuffer(buffer);

						if (nullptr != reader)
						{
							byte * uselessArray = new byte[buffer->Length];

							reader->ReadBytes(Platform::ArrayReference<byte>(uselessArray, buffer->Length));

							if (static_cast<size_t>(buffer->Length) == fwrite(uselessArray, sizeof(byte), static_cast<size_t>(buffer->Length), filePointer))
							{
								if (0 == fseek(filePointer, 0L, SEEK_SET))
								{
									delete[] uselessArray;
									delete reader;
									reader = nullptr;
									return filePointer;
								}
							}
							delete[] uselessArray;
							delete reader;
							reader = nullptr;
						}*/
					}
					else
					{
						wchar_t errorText[94] = { L'\0' };

						__wcserror_s(errorText, NULL);

						_close(fd); // Also calls CloseHandle()
					}
				}
				else
				{
					hr = TYPE_E_IOERROR;

					CloseHandle(tempFileHandle);
				}
			}
			else
			{
				hr = HRESULT_FROM_WIN32(GetLastError());
			}
		}

		return filePointer;
	});
}

/*
* ERROR HANDLING:
*
* The JPEG library's standard error handler (jerror.c) is divided into
* several "methods" which you can override individually.  This lets you
* adjust the behavior without duplicating a lot of code, which you might
* have to update with each future release.
*
* Our example here shows how to override the "error_exit" method so that
* control is returned to the library's caller when a fatal error occurs,
* rather than calling exit() as the standard error_exit method does.
*
* We use C's setjmp/longjmp facility to return control.  This means that the
* routine which calls the JPEG library must first execute a setjmp() call to
* establish the return point.  We want the replacement error_exit to do a
* longjmp().  But we need to make the setjmp buffer accessible to the
* error_exit routine.  To do this, we make a private extension of the
* standard JPEG error handler object.  (If we were using C++, we'd say we
* were making a subclass of the regular error handler.)
*
* Here's the extended error handler struct:
*/
struct my_error_mgr {
	struct jpeg_error_mgr pub;	/* "public" fields */

	jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

// The routine that will replace the standard error_exit method
METHODDEF(void) my_error_exit(j_common_ptr cinfo)
{
	// cinfo->err really points to a my_error_mgr struct, so coerce pointer
	my_error_ptr myerr = (my_error_ptr)cinfo->err;

	// Note: Can postpone this until after returning
	char buffer[JMSG_LENGTH_MAX];

	// Create the message
	(*cinfo->err->format_message) (cinfo, buffer);

	// Return control to the setjmp point
	longjmp(myerr->setjmp_buffer, 1);
}

concurrency::task<HRESULT> CreateReorientedTempFileAsync(Item^ item, size_t maxMemoryToUse, BOOL trim = FALSE, BOOL progressive = TRUE)
{
	return concurrency::create_task(StorageFileToFilePointerAsync(item->StorageFile))
		.then([item, maxMemoryToUse, trim, progressive](FILE * fp)
	{
		struct jpeg_decompress_struct srcinfo;
		struct jpeg_compress_struct dstinfo;

		/* We use our private extension JPEG error handler.
		* Note that this struct must live as long as the main JPEG parameter
		* struct, to avoid dangling-pointer problems.
		*/
		struct my_error_mgr jsrcerr, jdsterr;

		jvirt_barray_ptr * src_coef_arrays;
		jvirt_barray_ptr * dst_coef_arrays;

		/* We assume all-in-memory processing and can therefore use only a
		* single file pointer for sequential input and output operation.
		*/

		jpeg_transform_info transformoption;
		JCOPY_OPTION copyoption = JCOPYOPT_ALL;

		switch (item->OrientationCoalesced) {
		case 2U:
		{
			transformoption.transform = JXFORM_FLIP_H;
		}
		break;
		case 3U:
		{
			transformoption.transform = JXFORM_ROT_180;
		}
		break;
		case 4U:
		{
			transformoption.transform = JXFORM_FLIP_V;
		}
		break;
		case 5U:
		{
			transformoption.transform = JXFORM_TRANSPOSE;
		}
		break;
		case 6U:
		{
			transformoption.transform = JXFORM_ROT_90;
		}
		break;
		case 7U:
		{
			transformoption.transform = JXFORM_TRANSVERSE;
		}
		break;
		case 8U:
		{
			transformoption.transform = JXFORM_ROT_270;
		}
		break;
		default:
		{
			transformoption.transform = JXFORM_NONE;
		}
		break;
		}

		transformoption.perfect = (trim == TRUE) ? FALSE : TRUE;
		transformoption.trim = (trim == TRUE) ? TRUE : FALSE;
		transformoption.force_grayscale = FALSE;
		transformoption.crop = FALSE;

		// Initialize the JPEG decompression object with default error handling
		srcinfo.err = jpeg_std_error(&jsrcerr.pub);

		// Set up the normal JPEG error routines, then override error_exit
		jsrcerr.pub.error_exit = my_error_exit;

		// Establish the setjmp return context for my_error_exit to use
		if (setjmp(jsrcerr.setjmp_buffer))
		{
			// IThe JPEG code has signaled an error. Clean up the JPEG object,
			jpeg_destroy_decompress(&srcinfo);
			jpeg_destroy_compress(&dstinfo);
			// close the input file,
			fclose(fp);
			// and return
			return E_FAIL;
		}

		jpeg_create_decompress(&srcinfo);

		// Initialize the JPEG compression object with default error handling
		dstinfo.err = jpeg_std_error(&jdsterr.pub);

		jdsterr.pub.error_exit = my_error_exit;

		if (setjmp(jdsterr.setjmp_buffer))
		{
			// IThe JPEG code has signaled an error. Clean up the JPEG object,
			jpeg_destroy_compress(&dstinfo);
			jpeg_destroy_decompress(&srcinfo);
			// close the input file,
			fclose(fp);
			// and return
			return E_FAIL;
		}

		jpeg_create_compress(&dstinfo);

		dstinfo.optimize_coding = TRUE;
		dstinfo.err->trace_level = 0;
		srcinfo.mem->max_memory_to_use = maxMemoryToUse;
		dstinfo.mem->max_memory_to_use = srcinfo.mem->max_memory_to_use;

		// Specify data source for decompression
		jpeg_stdio_src(&srcinfo, fp);

		// Enable saving of extra markers that we want to copy
		jcopy_markers_setup(&srcinfo, copyoption);

		// Read file header
		/* We can ignore the return value from jpeg_read_header since
		*   (a) suspension is not possible with the stdio data source, and
		*   (b) we passed TRUE to reject a tables-only JPEG file as an error.
		* See libjpeg.txt for more info.
		*/
		(void)jpeg_read_header(&srcinfo, TRUE);

		if (!jtransform_request_workspace(&srcinfo, &transformoption))
		{
			jpeg_destroy_compress(&dstinfo);
			jpeg_destroy_decompress(&srcinfo);

			fclose(fp);

			return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
		}

		// Read source file as DCT coefficients
		src_coef_arrays = jpeg_read_coefficients(&srcinfo);

		// Initialize destination compression parameters from source values
		jpeg_copy_critical_parameters(&srcinfo, &dstinfo);

		/* Adjust destination parameters if required by transform options;
		* also find out which set of coefficient arrays will hold the output.
		*/
		dst_coef_arrays = jtransform_adjust_parameters(&srcinfo, &dstinfo, src_coef_arrays, &transformoption);

		// Close input file
		/* Note: we assume that jpeg_read_coefficients consumed all input
		* until JPEG_REACHED_EOI, and that jpeg_finish_decompress will
		* only consume more while (! cinfo->inputctl->eoi_reached).
		* We cannot call jpeg_finish_decompress here since we still need the
		* virtual arrays allocated from the source object for processing.
		*/
		fclose(fp);

		concurrency::interruption_point();

		fp = CreateTempFile(item->TempFilePath->Data());

		if (nullptr == fp)
		{
			jpeg_destroy_compress(&dstinfo);
			jpeg_destroy_decompress(&srcinfo);

			return HRESULT_FROM_WIN32(GetLastError());
		}

		if (progressive)
		{
			// Set progressive mode (saves space but is slower)
			jpeg_simple_progression(&dstinfo);
		}

		// Specify data destination for compression
		jpeg_stdio_dest(&dstinfo, fp);

		// Start compressor (note no image data is actually written here)
		jpeg_write_coefficients(&dstinfo, dst_coef_arrays);

		// Copy to the output file any extra markers that we want to preserve
		jcopy_markers_execute(&srcinfo, &dstinfo, copyoption);

		// Execute image transformation, if any
		jtransform_execute_transformation(&srcinfo, &dstinfo, src_coef_arrays, &transformoption);

		// Finish compression
		jpeg_finish_compress(&dstinfo);
		// Release memory
		jpeg_destroy_compress(&dstinfo);
		// We can ignore the return value since suspension is not possible with the stdio data source
		(void)jpeg_finish_decompress(&srcinfo);
		jpeg_destroy_decompress(&srcinfo);

		// Close output file, also calls _close()
		fclose(fp);

		/*// Check whether any corrupt-data warnings occurred
		if (0L == srcinfo.err->num_warnings > 0L &&
			0L == dstinfo.err->num_warnings > 0L)
		{
			return S_OK;
		}
		else
		{
			return HRESULT_FROM_WIN32(ERROR_FILE_CORRUPT);
		}*/
		return S_OK;

	}, concurrency::task_continuation_context::use_arbitrary());
}

Scenario_AfterPick::Scenario_AfterPick()
{
    InitializeComponent();

	_dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

	_resourceLoader = Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView();

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr))
	{
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	}

	if (SUCCEEDED(hr))
	{
		CoInitializeExSucceeded = true;

		MULTI_QI mutliQi = { &__uuidof(pIWICImagingFactory), nullptr, S_OK };

		hr = CoCreateInstanceFromApp(
			CLSID_WICImagingFactory,
			nullptr,
			CLSCTX_INPROC_SERVER,
			nullptr,
			1,
			&mutliQi);

		if (SUCCEEDED(hr))
		{
			hr = mutliQi.hr;

			if (SUCCEEDED(hr))
			{
				pIWICImagingFactory = static_cast<IWICImagingFactory*>(mutliQi.pItf);
			}
		}

		//Windows::ApplicationModel::Package^ package = Windows::ApplicationModel::Package::Current;
		//Windows::Storage::StorageFolder^ installedLocation = package->InstalledLocation;
		//Windows::Storage::StorageFolder^ localFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
		//Windows::Storage::StorageFolder^ temporaryFolder = Windows::Storage::ApplicationData::Current->TemporaryFolder;

		//InputTextBlock1->Text = "Installed location:\n" + installedLocation->Path + "\n" + "Local folder:\n" + localFolder->Path;
		//InputTextBlock1->Text = temporaryFolder->Path;
	}

	if (FAILED(hr))
	{
		InputTextBlock1->Text = HResultToHexString(hr);
	}

	imagesSelected = 0UL;
	imagesAnalysed = 0UL;
	imagesToBeRotated = 0UL;
	imagesRotated = 0UL;
	imagesErrored = 0UL;
	imagesBeingRotated = 0UL;

	_updateStatusTextTimer = nullptr;
	_stoppingUpdateStatusText = false;

	// Sensible default of one processor
	numberProcessorsToUse = 1UL;

	concurrency::create_task(LoadSettingAsync("numberLogicalProcessorsToUse"))
		.then([this](IPropertyValue^ value)
	{
		if (nullptr != value)
		{
			numberProcessorsToUse = value->GetUInt32();
		}
	});

	// Sensible default of 512 MiB
	bytesRAMToUse = 512ULL * 1024ULL * 1024ULL;

	concurrency::create_task(LoadSettingAsync("megabytesRAMToUse"))
		.then([this](IPropertyValue^ value)
	{
		if (nullptr != value)
		{
			bytesRAMToUse = value->GetUInt64() * 1024ULL * 1024ULL;
		}
	});
}

Scenario_AfterPick::~Scenario_AfterPick()
{
	SafeRelease(&pIWICImagingFactory);

	if (CoInitializeExSucceeded)
	{
		CoUninitialize();
	}

	if (_updateStatusTextTimer)
	{
		_updateStatusTextTimer->Cancel();
	}
}

void Scenario_AfterPick::UpdateStatusText()
{
	Platform::String^ statusString;

	// If you are still analysing
	if (imagesAnalysed.load() < imagesSelected.load())
	{
		// Capture the current value so it can't mutate during this call
		auto imagesAnalysedCurrent = imagesAnalysed.load();

		if (imagesAnalysedCurrent > 1UL)
		{
			statusString = _resourceLoader->GetString("analysedMany");

			// Subtract one length of "%lu", add 1U for null terminator
			unsigned int stringLength = statusString->Length() + imagesAnalysedCurrent.ToString()->Length() - 3U + 1U;

			wchar_t * wstring = new wchar_t[stringLength];

			if (-1 != swprintf_s(wstring, stringLength, statusString->Data(), imagesAnalysedCurrent))
			{
				statusString = ref new Platform::String(wstring);
			}

			delete[] wstring;
		}
		else
		{
			statusString = _resourceLoader->GetString("analysedOne");
		}
	}
	// If all images have been analysed, imagesToBeRotated will have its final value
	else
	{
		// If images are still being processed
		if (imagesRotated.load() + imagesErrored.load() < imagesToBeRotated.load())
		{
			// Capture the current values so they can't mutate during this call
			auto imagesRotatedCurrent = imagesRotated.load();
			auto imagesToBeRotatedCurrent = imagesToBeRotated.load();
			auto imagesErroredCurrent = imagesErrored.load();

			if (imagesToBeRotatedCurrent > 1UL || (1UL == imagesErroredCurrent && 1UL == imagesToBeRotatedCurrent))
			{
				statusString = _resourceLoader->GetString("processedMany");

				// Subtract two lengths of "%lu", add 1U for null terminator
				unsigned int stringLength = statusString->Length() + imagesRotatedCurrent.ToString()->Length() + imagesToBeRotatedCurrent.ToString()->Length() - (2 * 3U) + 1U;

				wchar_t * wstring = new wchar_t[stringLength];

				if (-1 != swprintf_s(wstring, stringLength, statusString->Data(), imagesRotatedCurrent, imagesToBeRotatedCurrent))
				{
					statusString = ref new Platform::String(wstring);
				}

				delete[] wstring;
			}
			else
			{
				statusString = _resourceLoader->GetString("processedOne");
			}
		}
		// If all images have been processed
		else
		{
			// If this is the first time you are here
			if (!_stoppingUpdateStatusText.load())
			{
				_stoppingUpdateStatusText.store(true);

				_dispatcher->RunAsync(
					Windows::UI::Core::CoreDispatcherPriority::High,
					ref new Windows::UI::Core::DispatchedHandler([this]()
				{
					// Flip the 'Cancel' button to 'Select Files'
					rootPage->FlipButton();

					// Stop the logo spinning animation
					rootPage->SpinLogo_Stop();
				}));

				_updateStatusTextTimer->Cancel();
			}
			else
			{
				// short-circuit
				return;
			}

			// if there's nothing to rotate
			if (0UL == imagesToBeRotated.load())
			{
				// say so
				statusString = _resourceLoader->GetString("noImagesToBeRotated");
			}
			// if you've rotated all images without errors
			else if (imagesRotated.load() == imagesToBeRotated.load())
			{
				if (imagesRotated.load() > 1UL)
				{
					statusString = _resourceLoader->GetString("processedAllMany");

					// Subtract one length of "%lu", add 1U for null terminator
					unsigned int stringLength = statusString->Length() + imagesRotated.load().ToString()->Length() - 3U + 1U;

					wchar_t * wstring = new wchar_t[stringLength];

					if (-1 != swprintf_s(wstring, stringLength, statusString->Data(), imagesRotated.load()))
					{
						statusString = ref new Platform::String(wstring);
					}

					delete[] wstring;
				}
				else
				{
					statusString = _resourceLoader->GetString("processedAllOne");
				}
			}
			// If there have been errors
			else
			{
				if (imagesToBeRotated > 1UL || (1UL == imagesErrored && 1UL == imagesToBeRotated))
				{
					statusString = _resourceLoader->GetString("processedMany");

					// Subtract two lengths of "%lu", add 1U for null terminator
					unsigned int stringLength = statusString->Length() + imagesRotated.load().ToString()->Length() + imagesToBeRotated.load().ToString()->Length() - (2 * 3U) + 1U;

					wchar_t * wstring = new wchar_t[stringLength];

					if (-1 != swprintf_s(wstring, stringLength, statusString->Data(), imagesRotated.load(), imagesToBeRotated.load()))
					{
						statusString = ref new Platform::String(wstring);
					}

					delete[] wstring;
				}
				else
				{
					statusString = _resourceLoader->GetString("processedOne");
				}
			}
		}
	}

	_dispatcher->RunAsync(
		Windows::UI::Core::CoreDispatcherPriority::Normal,
		ref new Windows::UI::Core::DispatchedHandler([this, statusString]()
	{
		InputTextBlock1->Text = statusString;
	}));
}

Windows::Foundation::TimeSpan HertzToTimeSpan(double hertz)
{
	Windows::Foundation::TimeSpan timeSpan;

	// 10,000,000 ticks per second
	timeSpan.Duration = static_cast<long long>(10000000.0 / hertz);

	return timeSpan;
}

// <summary>
// Invoked when this page is about to be displayed in a Frame.
// </summary>
// <param name="e">Event data that describes how this page was reached. The Parameter
// property is typically used to configure the page.</param>
void Scenario_AfterPick::OnNavigatedTo(NavigationEventArgs^ e)
{
	// If there is any text in the top section then there has been an error
	if (!InputTextBlock1->Text->IsEmpty())
	{
		// so return
		return;
	}

    // A pointer back to the main page. This is needed if you want to call methods in MainPage
    rootPage = MainPage::Current;

	auto cancellationToken = (reinterpret_cast<concurrency::cancellation_token_source*>(rootPage->cts->Value))->get_token();

	auto openPicker = ref new Windows::Storage::Pickers::FolderPicker();

	openPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::PicturesLibrary;

	openPicker->ViewMode = Windows::Storage::Pickers::PickerViewMode::Thumbnail;

	// Filter to include a sample subset of file types.
	openPicker->FileTypeFilter->Clear();
	openPicker->FileTypeFilter->Append(".jpg");
	openPicker->FileTypeFilter->Append(".jpeg");
	openPicker->FileTypeFilter->Append(".jpe");
	openPicker->FileTypeFilter->Append(".jif");
	openPicker->FileTypeFilter->Append(".jfif");
	openPicker->FileTypeFilter->Append(".jfi");

	// All this work will be done asynchronously on a background thread:
	// Wrap the async call inside a concurrency::task object
	auto pickerTask = concurrency::create_task(openPicker->PickSingleFolderAsync());

	pickerTask.then([this, cancellationToken, openPicker](Windows::Storage::StorageFolder^ folder)
	{
		// folder is null if user cancels the folder picker
		if (nullptr == folder)
		{
			// Stop work and clean up.
			concurrency::cancel_current_task();
		}

		storeData = ref new Data();

		GridView1->ItemsSource = storeData->Items;

		// Create query based on the previous file type filter
		auto queryOptions = ref new Windows::Storage::Search::QueryOptions(Windows::Storage::Search::CommonFileQuery::DefaultQuery, openPicker->FileTypeFilter);

		auto storageFileQueryResult = folder->CreateFileQueryWithOptions(queryOptions);

		auto getFilesAsyncTask = concurrency::create_task(storageFileQueryResult->GetFilesAsync());

		getFilesAsyncTask.then([this, cancellationToken](IVectorView<Windows::Storage::StorageFile^>^ files)
		{
			imagesSelected = static_cast<unsigned long>(files->Size);

			// if there are no JPEG files in the folder
			if (0UL == imagesSelected)
			{
				// Stop work and clean up.
				concurrency::cancel_current_task();
			}

			rootPage->FlipButton();

			rootPage->SpinLogo_Start();

			InputTextBlock1->Text = _resourceLoader->GetString("initialising");		

			concurrency::create_task([this]
			{
				// wait for all the images to be analysed
				while (imagesAnalysed.load() < imagesSelected.load())
				{
					concurrency::interruption_point();

					Sleep(20);
				}

				// we got here if all images have been analysed, hence imagesToBeRotated will have its final value
				if (0UL == imagesToBeRotated.load())
				{
					concurrency::cancel_current_task();
				}

				return;
			}, cancellationToken)
				.then([this, cancellationToken]()
			{
				// Update the UI thread by using the UI core dispatcher
				_dispatcher->RunAsync(
					Windows::UI::Core::CoreDispatcherPriority::High,
					ref new Windows::UI::Core::DispatchedHandler([this]()
				{
					// if selections are not enabled in the grid
					if (Windows::UI::Xaml::Controls::ListViewSelectionMode::Extended != GridView1->SelectionMode)
					{
						// enable them
						GridView1->SelectionMode = Windows::UI::Xaml::Controls::ListViewSelectionMode::Extended;
					}
				}));

				for (unsigned int i = 0U; i < static_cast<unsigned int>(imagesToBeRotated.load()); ++i)
				{
					concurrency::interruption_point();

					Item^ item = nullptr;

					// wait for the data behind the UI grid to populate
					while (nullptr == item)
					{
						try
						{
							item = safe_cast<Item^>(storeData->Items->GetAt(i));
						}
						catch (Platform::Exception^ e)
						{
							if (E_BOUNDS == e->HResult)
							{
								Sleep(20);
							}
							else
							{
								_dispatcher->RunAsync(
									Windows::UI::Core::CoreDispatcherPriority::Low,
									ref new Windows::UI::Core::DispatchedHandler([this, item, e]()
								{
									item->Error = HResultToHexString(e->HResult);

									Windows::UI::Xaml::Controls::Primitives::SelectorItem^ sI = safe_cast<Windows::UI::Xaml::Controls::Primitives::SelectorItem^>(GridView1->ContainerFromItem(item));

									// this can return a nullptr when the item has not yet been rendered to the grid - this is normal!
									// when the item is due to be rendered, the ShowError() will get called on it anyway
									if (nullptr != sI)
									{
										ItemViewer^ iv = safe_cast<ItemViewer^>(sI->ContentTemplateRoot);

										if (nullptr != iv)
										{
											iv->ShowError();
										}
									}
								}));
							}
						}
					}

					concurrency::interruption_point();

					if (!item->Error->IsEmpty())
					{
						imagesErrored++;

						continue;
					}

					while (imagesBeingRotated >= numberProcessorsToUse)
					{
						Sleep(20);
					}

					imagesBeingRotated++;

					auto createReorientedTempFileAsyncTask = CreateReorientedTempFileAsync(
						item,
						static_cast<size_t>(
						(static_cast<double>(0.95) * static_cast<double>(bytesRAMToUse)) / static_cast<double>(numberProcessorsToUse)
						),
						rootPage->CropChecked,
						rootPage->ProgressiveChecked);

					createReorientedTempFileAsyncTask.then([this, cancellationToken, item](HRESULT hr)
					{
						imagesBeingRotated--;

						if (SUCCEEDED(hr))
						{
							concurrency::interruption_point();

							hr = FixMetadata(item, pIWICImagingFactory);

							if (SUCCEEDED(hr))
							{
								concurrency::interruption_point();

								hr = FixMetadataOutOfPlace(item, pIWICImagingFactory);
							}

								if (SUCCEEDED(hr))
								{
									//if (rootPage->DeleteThumbnails) {DeleteJPEGThumbnailData(item, pIWICImagingFactory);}

									concurrency::interruption_point();

									//Sleep((((rand() % 100) + 1) / 100.0) * 1000.0);

									auto getFileFromPathAsyncTask = concurrency::create_task(Windows::Storage::StorageFile::GetFileFromPathAsync(item->TempFilePath));

									getFileFromPathAsyncTask.then([this, cancellationToken, item](Windows::Storage::StorageFile^ tempFile)
									{
										concurrency::interruption_point();

										auto readBufferAsyncTask = concurrency::create_task(Windows::Storage::FileIO::ReadBufferAsync(tempFile));

										readBufferAsyncTask.then([this, cancellationToken, item, tempFile](Windows::Storage::Streams::IBuffer^ tempFileBuffer)
										{
											concurrency::interruption_point();

											auto openAsyncTask = concurrency::create_task(item->StorageFile->OpenAsync(Windows::Storage::FileAccessMode::ReadWrite));

											openAsyncTask.then([this, item, tempFile, tempFileBuffer](Windows::Storage::Streams::IRandomAccessStream^ randomAccessStream)
											{
												concurrency::interruption_point();

												Windows::Storage::Streams::DataWriter^ dataWriter = ref new Windows::Storage::Streams::DataWriter(randomAccessStream);

												dataWriter->WriteBuffer(tempFileBuffer);

												concurrency::interruption_point();

												auto storeAsyncTask = concurrency::create_task(dataWriter->StoreAsync());
											
												storeAsyncTask.then([this, item, tempFile, randomAccessStream, dataWriter](concurrency::task<unsigned int> task)
												{
													(void)concurrency::create_task(tempFile->DeleteAsync(Windows::Storage::StorageDeleteOption::PermanentDelete));

													unsigned int bytesWritten = 0U;

													try
													{
														bytesWritten = task.get();
													}
													catch (Platform::Exception^ ex)
													{
														imagesErrored++;

														_dispatcher->RunAsync(
															Windows::UI::Core::CoreDispatcherPriority::Low,
															ref new Windows::UI::Core::DispatchedHandler([this, item, ex]()
														{
															item->Error = HResultToHexString(ex->HResult);

															Windows::UI::Xaml::Controls::Primitives::SelectorItem^ sI = safe_cast<Windows::UI::Xaml::Controls::Primitives::SelectorItem^>(GridView1->ContainerFromItem(item));

															// this can return a nullptr when the item has not yet been rendered to the grid - this is normal!
															// when the item is due to be rendered, the ShowError() will get called on it anyway
															if (nullptr != sI)
															{
																ItemViewer^ iv = safe_cast<ItemViewer^>(sI->ContentTemplateRoot);

																if (nullptr != iv)
																{
																	iv->ShowError();
																}
															}
														}));

														return;
													}

													// reset stream size to override the file
													randomAccessStream->Size = bytesWritten;

													imagesRotated++;

													// Make changes on the UI thread
													_dispatcher->RunAsync(
														Windows::UI::Core::CoreDispatcherPriority::Low,
														ref new Windows::UI::Core::DispatchedHandler([this, item]()
													{
														// Mark the item's orientation as rotated
														if (0U != item->Orientation)
														{
															item->Orientation = 1U;
														}

														if (0U != item->OrientationXMP)
														{
															item->OrientationXMP = 1U;
														}

														// Mark the item as selected in the grid
														GridView1->SelectedItems->Append(item);

														Windows::UI::Xaml::Controls::Primitives::SelectorItem^ sI = safe_cast<Windows::UI::Xaml::Controls::Primitives::SelectorItem^>(GridView1->ContainerFromItem(item));

														// this can return a nullptr when the item has not yet been rendered to the grid - this is normal!
														// when the item is due to be rendered, the ShowImage() will get called on it anyway
														if (nullptr != sI)
														{
															ItemViewer^ iv = safe_cast<ItemViewer^>(sI->ContentTemplateRoot);

															if (nullptr != iv)
															{
																iv->ShowImage();
															}
														}
													}));
												});
											}, cancellationToken);
										}, cancellationToken);
									}, cancellationToken);
								}
								// If cannot fix the metadata
								else
								{
									imagesErrored++;

									_dispatcher->RunAsync(
										Windows::UI::Core::CoreDispatcherPriority::Low,
										ref new Windows::UI::Core::DispatchedHandler([this, item, hr]()
									{
										item->Error = HResultToHexString(hr);

										Windows::UI::Xaml::Controls::Primitives::SelectorItem^ sI = safe_cast<Windows::UI::Xaml::Controls::Primitives::SelectorItem^>(GridView1->ContainerFromItem(item));

										// this can return a nullptr when the item has not yet been rendered to the grid - this is normal!
										// when the item is due to be rendered, the ShowError() will get called on it anyway
										if (nullptr != sI)
										{
											ItemViewer^ iv = safe_cast<ItemViewer^>(sI->ContentTemplateRoot);

											if (nullptr != iv)
											{
												iv->ShowError();
											}
										}
									}));
								};
						}
						// If cannot create re-oriented temporary file
						else
						{
							imagesErrored++;

							_dispatcher->RunAsync(
								Windows::UI::Core::CoreDispatcherPriority::Low,
								ref new Windows::UI::Core::DispatchedHandler([this, item, hr]()
							{
								if (HRESULT_FROM_WIN32(ERROR_INVALID_DATA) == hr)
								{
									item->Error = _resourceLoader->GetString("cropOptionRequired");
								}
								else
								{
									item->Error = HResultToHexString(hr);
								}

								Windows::UI::Xaml::Controls::Primitives::SelectorItem^ sI = safe_cast<Windows::UI::Xaml::Controls::Primitives::SelectorItem^>(GridView1->ContainerFromItem(item));

								// this can return a nullptr when the item has not yet been rendered to the grid - this is normal!
								// when the item is due to be rendered, the ShowError() will get called on it anyway
								if (nullptr != sI)
								{
									ItemViewer^ iv = safe_cast<ItemViewer^>(sI->ContentTemplateRoot);

									if (nullptr != iv)
									{
										iv->ShowError();
									}
								}
							}));
						}
					}, cancellationToken);
				}							
			},
				cancellationToken,
				concurrency::task_continuation_context::use_arbitrary()
				);

			// Poll for updates via a periodic timer
			_updateStatusTextTimer = Windows::System::Threading::ThreadPoolTimer::CreatePeriodicTimer(
				ref new Windows::System::Threading::TimerElapsedHandler([this](Windows::System::Threading::ThreadPoolTimer^ source)
			{
				this->UpdateStatusText();
			}),
				// Set duration corresponding to 60 Hz (maximum cone flicker fusion frequency)
				HertzToTimeSpan(60.0)
				);

			for (unsigned int i = 0U; i < files->Size; ++i)
			{
				concurrency::interruption_point();

				Item^ item = ref new Item();

				item->StorageFile = files->GetAt(i);

				auto getMetadataAsyncTask = concurrency::create_task(GetMetadataAsync(item, pIWICImagingFactory));

				getMetadataAsyncTask.then([this, item, cancellationToken](HRESULT hr)
				{
					concurrency::interruption_point();

					if (SUCCEEDED(hr))
					{
						if (item->OrientationCoalesced >= 2U && item->OrientationCoalesced <= 8U)
						{
							concurrency::interruption_point();

							imagesToBeRotated++;

							auto getThumbnailTask = concurrency::create_task(item->StorageFile->GetThumbnailAsync(Windows::Storage::FileProperties::ThumbnailMode::SingleItem, 192U));

							getThumbnailTask.then([this, item, cancellationToken](Windows::Storage::FileProperties::StorageItemThumbnail^ thumbnail)
							{
								concurrency::interruption_point();

								Windows::UI::Xaml::Media::Imaging::BitmapImage^ bitmapImage = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage();

								// Set the stream as source of the bitmap
								bitmapImage->SetSource(thumbnail);

								item->UUID = GetUUID();

								// Add picked file to MostRecentlyUsedList
								//item->MRUToken = Windows::Storage::AccessCache::StorageApplicationPermissions::MostRecentlyUsedList->Add(item->StorageFile);

								if (Windows::Storage::FileAttributes::ReadOnly == (item->StorageFile->Attributes & Windows::Storage::FileAttributes::ReadOnly))
								{
									item->Error = _resourceLoader->GetString("imageReadOnly");
								}

								// Set the bitmap as source of the Image control
								item->Image = bitmapImage;

								storeData->Items->Append(item);

							}, cancellationToken);
						}
					}

					// Important to increment this after the to-be-rotated count so that the monitor jobs don't race
					imagesAnalysed++;

				}, cancellationToken);
			}
		}, cancellationToken);
	}, cancellationToken);
}

// We will visualize the data item in asynchronously in multiple phases for improved panning user experience 
// of large lists.  We will visualize different parts of the data item in the following order:
// 
//     1) Placeholders (visualized synchronously - Phase 0)
//     2) Title (visualized asynchronously - Phase 1)
//     3) Error and Image (visualized asynchronously - Phase 2)
void Scenario_AfterPick::ItemGridView_ContainerContentChanging(
    ListViewBase^ sender,
    Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs^ args
	)
{
    ItemViewer^ iv = safe_cast<ItemViewer^>(args->ItemContainer->ContentTemplateRoot);

    if (iv != nullptr)
    {
        // if the container is being added to the recycle queue (meaning it will not particiapte in 
        // visualizing data items for the time being), we clear out the data item
        if (args->InRecycleQueue)
        {
            iv->ClearData();
        }
        else if (args->Phase == 0U)
        {
            iv->ShowPlaceholder(safe_cast<Item^>(args->Item));

            // Register for async callback to visualize Title asynchronously
            args->RegisterUpdateCallback(ContainerContentChangingDelegate);
        }
        else if (args->Phase == 1U)
        {
            iv->ShowTitle();

            args->RegisterUpdateCallback(ContainerContentChangingDelegate);
        }
        else if (args->Phase == 2U)
        {
            iv->ShowError();
            iv->ShowImage();
        }

        args->Handled = true;
    }
}
