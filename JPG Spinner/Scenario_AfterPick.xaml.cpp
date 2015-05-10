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
// Scenario1.xaml.cpp
// Implementation of the Scenario1 class
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
		&extendedParams
		);

	if (INVALID_HANDLE_VALUE == hTempFile)
	{
		throw Platform::Exception::CreateException(HRESULT_FROM_WIN32(GetLastError()));
	}

	// Open the output file from the handle
	int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hTempFile), _O_RDWR | _O_BINARY);

	if (-1 == fd)
	{
		CloseHandle(hTempFile);

		throw Platform::Exception::CreateException(HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE));
	}

	FILE * fp = _fdopen(fd, "w+b");

	if (0 == fp)
	{
		// Also calls CloseHandle()
		_close(fd);

		throw Platform::Exception::CreateException(HRESULT_FROM_WIN32(ERROR_CANNOT_MAKE));
	}

	return fp;
}

HRESULT GetMetadata(IStream * FileStream, Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;

	HRESULT hr = pIWICImagingFactory->CreateDecoderFromStream(
		FileStream,
		NULL,
		WICDecodeMetadataCacheOnLoad,
		&pDecoder
		);

	if (FAILED(hr))
	{
		// Try to load without caching metadata
		hr = pIWICImagingFactory->CreateDecoderFromStream(
			FileStream,
			NULL,
			WICDecodeMetadataCacheOnDemand,
			&pDecoder
			);
	}

	GUID guidContainerFormat = GUID_NULL;

	hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	if (FAILED(hr)) return hr;

	if (GUID_ContainerFormatJpeg != guidContainerFormat)
	{
		return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;

	hr = pDecoder->GetFrame(0U, &pSource);
	if (FAILED(hr)) return hr;

	Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pQueryReader;

	hr = pSource->GetMetadataQueryReader(&pQueryReader);
	if (FAILED(hr)) return hr;

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

	hrLastSuccessful = hr;

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

	hrLastSuccessful = hr;

	//SubjectArea
	hr = pQueryReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=37396}", &propVariant);

	if (SUCCEEDED(hr))
	{
		item->PtrSubjectArea = ref new Platform::Box<intptr_t>((intptr_t)&propVariant);
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
		//item->FocalPlaneXResolution = ref new Platform::Box<intptr_t>((intptr_t)&propVariant); //propVariant.uhVal;
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
		//item->FocalPlaneYResolution = ref new Platform::Box<intptr_t>((intptr_t)&propVariant);
	}
	else
	{
		hr = hrLastSuccessful;
	}

	PropVariantClear(&propVariant);

	//ThumbnailData
	//hr = pQueryReader->GetMetadataByName(L"/app0/{ushort=6}", &propvariantOrientationFlag);
	//WINCODEC_ERR_PROPERTYNOTFOUND

	//SubjectLocation
	//hr = pQueryReader->GetMetadataByName(L"/app1/ifd/exif/{ushort=41492}", &propvariantOrientationFlag);

	return hr;
}

// Zero the thumbnail App1/1st IFD block
HRESULT DeleteJPEGThumbnailMetadata(Item^ item, IWICImagingFactory * pIWICImagingFactory)
{
	Microsoft::WRL::ComPtr<IWICBitmapDecoder> pDecoder;

	HRESULT hr = pIWICImagingFactory->CreateDecoderFromFilename(
		item->TempFilePath->Data(),
		NULL,
		GENERIC_READ | GENERIC_WRITE,
		WICDecodeMetadataCacheOnDemand, // A decoder must be created using the WICDecodeOptions value WICDecodeMetadataCacheOnDemand to perform in-place metadata updates
		&pDecoder);
	if (FAILED(hr)) return hr;

	GUID guidContainerFormat = GUID_NULL;

	hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	if (FAILED(hr)) return hr;

	if (guidContainerFormat != GUID_ContainerFormatJpeg)
	{
		return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;

	hr = pDecoder->GetFrame(0, &pSource);
	if (FAILED(hr)) return hr;

	// Set the orientation value to normal
	Microsoft::WRL::ComPtr<IWICFastMetadataEncoder> pFME;

	hr = pIWICImagingFactory->CreateFastMetadataEncoderFromFrameDecode(pSource.Get(), &pFME);
	if (FAILED(hr)) return hr;

	Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> pFMEQW;

	hr = pFME->GetMetadataQueryWriter(&pFMEQW);
	if (FAILED(hr)) return hr;

	Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pQueryReader;

	hr = pSource->GetMetadataQueryReader(&pQueryReader);
	if (FAILED(hr)) return hr;

	Platform::String^ pathMetadata = "/app1/thumb";

	PROPVARIANT value = { 0 };
	PropVariantInit(&value);

	hr = pQueryReader->GetMetadataByName(pathMetadata->Data(), &value);
	if (FAILED(hr)) return hr;

	Microsoft::WRL::ComPtr<IWICMetadataQueryReader> pEmbedReader;

	hr = value.punkVal->QueryInterface(IID_PPV_ARGS(&pEmbedReader));
	if (FAILED(hr)) return hr;

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
	// Note that the thumbnail offset is at JPEGInterchangeFormat + 12U from the beginning of the file

	if (Platform::Guid(GUID_NULL) == item->UUID || 0U == item->JPEGInterchangeFormat || 0U == item->JPEGInterchangeFormatLength)
	{
		return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
	}

	HRESULT hr = DeleteJPEGThumbnailMetadata(item, pIWICImagingFactory);
	if (FAILED(hr)) return hr;

	Platform::String^ filePathSuffix = ".temp";

	FILE * tempFilePointer = CreateTempFile((item->TempFilePath + filePathSuffix)->Data());

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

	byte fileHead[65536] = { 0 };

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
	if (6 + lengthApp1 - 2 != fwrite(fileHead, sizeof(byte), 6 + lengthApp1 - 2, tempFilePointer))
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
	if (FAILED(hr)) return hr;

	GUID guidContainerFormat = GUID_NULL;

	hr = pDecoder->GetContainerFormat(&guidContainerFormat);
	if (FAILED(hr)) return hr;

	if (guidContainerFormat != GUID_ContainerFormatJpeg)
	{
		return WINCODEC_ERR_UNKNOWNIMAGEFORMAT;
	}

	Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> pSource;

	hr = pDecoder->GetFrame(0, &pSource);
	if (FAILED(hr)) return hr;

	// Set the orientation value to normal
	Microsoft::WRL::ComPtr<IWICFastMetadataEncoder> pFME;

	hr = pIWICImagingFactory->CreateFastMetadataEncoderFromFrameDecode(pSource.Get(), &pFME);
	if (FAILED(hr)) return hr;

	Microsoft::WRL::ComPtr<IWICMetadataQueryWriter> pFMEQW;

	hr = pFME->GetMetadataQueryWriter(&pFMEQW);
	if (FAILED(hr)) return hr;

	PROPVARIANT propvariantOrientationFlag = { 0 };
	PropVariantInit(&propvariantOrientationFlag);
	
	propvariantOrientationFlag.vt = VT_UI2;
	propvariantOrientationFlag.uiVal = 1U;

	if (0U != item->Orientation)
	{
		hr = pFMEQW->SetMetadataByName(L"/app1/ifd/{ushort=274}", &propvariantOrientationFlag);
		if (FAILED(hr)) return hr;
	}

	if (0U != item->OrientationXMP)
	{
		hr = pFMEQW->SetMetadataByName(L"/xmp/tiff:Orientation", &propvariantOrientationFlag);
		if (FAILED(hr)) return hr;
	}

	PropVariantClear(&propvariantOrientationFlag);

	// Set the SubjectArea value according to the re-oriented coordinates
	/*if (SUCCEEDED(hr))
	{
		PROPVARIANT propvariantToSet = { 0 };
		PropVariantInit(&propvariantToSet);

		hr = PropVariantCopy(&propvariantToSet, (PROPVARIANT*)item->PtrSubjectArea->Value);

		if (SUCCEEDED(hr))
		{
			auto coordinate = Windows::Foundation::Point(static_cast<float>(((PROPVARIANT*)item->PtrSubjectArea->Value)->caui.pElems[0]), static_cast<float>(((PROPVARIANT*)item->PtrSubjectArea->Value)->caui.pElems[1]));

			UINT width, height = 0U;

			hr = pSource->GetSize(&width, &height);

			if (SUCCEEDED(hr))
			{
				auto orientationMatrix = new OrientationMatrix(item->Orientation ? item->Orientation : item->OrientationXMP);

				auto matrix = orientationMatrix->getMatrix();

				if (orientationMatrix->XYFlips())
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

				if (4U == ((PROPVARIANT*)item->PtrSubjectArea->Value)->caui.cElems)
				{
					if (orientationMatrix->XYFlips())
					{
						propvariantToSet.caui.pElems[2] = ((PROPVARIANT*)item->PtrSubjectArea->Value)->caui.pElems[3];
						propvariantToSet.caui.pElems[3] = ((PROPVARIANT*)item->PtrSubjectArea->Value)->caui.pElems[2];
					}
				}

				// Commit chokes on this
				//hr = pFMEQW->SetMetadataByName(L"/app1/ifd/exif/{ushort=37396}", &propvariantToSet);
			}
		}

		PropVariantClear(&propvariantToSet);
	}*/

	hr = pFME->Commit();

	return hr;
}

static Platform::Guid GetUUID()
{
	GUID result;

	HRESULT hr = CoCreateGuid(&result);
	if (FAILED(hr)) throw Platform::Exception::CreateException(hr);

	return Platform::Guid(result);
}

byte* GetPointerToByteData(Windows::Storage::Streams::IBuffer^ buffer)
{
	byte* pixels = nullptr;

	Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;

	// Query the IBufferByteAccess interface.
	HRESULT hr = reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
	if (FAILED(hr)) throw Platform::Exception::CreateException(hr);

	// Retrieve the buffer data.
	hr = bufferByteAccess->Buffer(&pixels);
	if (FAILED(hr)) throw Platform::Exception::CreateException(hr);
	
	return pixels;
}

byte* GetPointerToByteData(Windows::Storage::Streams::IBuffer^ buffer, unsigned int *length)
{
	byte* pixels = nullptr;

	Microsoft::WRL::ComPtr<Windows::Storage::Streams::IBufferByteAccess> bufferByteAccess;

	// Query the IBufferByteAccess interface.
	HRESULT hr = reinterpret_cast<IInspectable*>(buffer)->QueryInterface(IID_PPV_ARGS(&bufferByteAccess));
	if (FAILED(hr)) throw Platform::Exception::CreateException(hr);

	// Retrieve the buffer data.
	hr = bufferByteAccess->Buffer(&pixels);
	if (FAILED(hr)) throw Platform::Exception::CreateException(hr);

	if (length != nullptr)
	{
		*length = buffer->Length;
	}
	
	return pixels;
}

concurrency::task<FILE *> StorageFileToFilePointer(Windows::Storage::StorageFile^ storageFile)
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
				int fd = _open_osfhandle((intptr_t)tempFileHandle, _O_RDWR | _O_BINARY);

				if (-1 != fd)
				{
					_set_errno(0);

					filePointer = _fdopen(fd, "w+b");

					if (nullptr != filePointer)
					{
						if (buffer->Length == fwrite(GetPointerToByteData(buffer), sizeof(byte), buffer->Length, filePointer))
						{
							if (0 == fseek(filePointer, 0L, SEEK_SET))
							{
								return filePointer;
							}
						}
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

concurrency::task<HRESULT> CreateReorientedTempFileAsync(Item^ item, BOOL trim = FALSE, BOOL progressive = TRUE)
{
	return concurrency::create_task(StorageFileToFilePointer(item->StorageFile))
		.then([item, trim, progressive](FILE * fp)
	{
		HRESULT hr = S_OK;

		struct jpeg_decompress_struct srcinfo;
		struct jpeg_compress_struct dstinfo;
		struct jpeg_error_mgr jsrcerr, jdsterr;

		jvirt_barray_ptr * src_coef_arrays;
		jvirt_barray_ptr * dst_coef_arrays;

		/* We assume all-in-memory processing and can therefore use only a
		* single file pointer for sequential input and output operation.
		*/

		jpeg_transform_info transformoption;
		JCOPY_OPTION copyoption = JCOPYOPT_ALL;

		switch (item->Orientation ? item->Orientation : item->OrientationXMP) {
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
		srcinfo.err = jpeg_std_error(&jsrcerr);
		jpeg_create_decompress(&srcinfo);
		// Initialize the JPEG compression object with default error handling
		dstinfo.err = jpeg_std_error(&jdsterr);
		jpeg_create_compress(&dstinfo);

		// Note: we assume only the decompression object will have virtual arrays

		dstinfo.optimize_coding = TRUE;
		dstinfo.err->trace_level = 0;
		//srcinfo.mem->max_memory_to_use = dstinfo.mem->max_memory_to_use;

		// Specify data source for decompression
		jpeg_stdio_src(&srcinfo, fp);

		// Enable saving of extra markers that we want to copy
		jcopy_markers_setup(&srcinfo, copyoption);

		// Read file header
		(void)jpeg_read_header(&srcinfo, TRUE);

		if (!jtransform_request_workspace(&srcinfo, &transformoption))
		{
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

		fp = CreateTempFile(item->TempFilePath->Data());

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

		// Finish compression and release memory
		jpeg_finish_compress(&dstinfo);
		jpeg_destroy_compress(&dstinfo);
		(void)jpeg_finish_decompress(&srcinfo);
		jpeg_destroy_decompress(&srcinfo);

		// Close output file, also calls _close()
		fclose(fp);

		return hr;
	});
}

Scenario_AfterPick::Scenario_AfterPick()
{
    InitializeComponent();

	_dispatcher = Windows::UI::Core::CoreWindow::GetForCurrentThread()->Dispatcher;

	_resourceLoader = Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView();

	pIWICImagingFactory = nullptr;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr))
	{
		hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	}

	if (SUCCEEDED(hr))
	{
		CoInitializeExSucceeded = true;

		hr = CoCreateInstance(
			CLSID_WICImagingFactory,
			NULL,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&pIWICImagingFactory)
			);

		//Windows::ApplicationModel::Package^ package = Windows::ApplicationModel::Package::Current;
		//Windows::Storage::StorageFolder^ installedLocation = package->InstalledLocation;
		//Windows::Storage::StorageFolder^ localFolder = Windows::Storage::ApplicationData::Current->LocalFolder;
		//Windows::Storage::StorageFolder^ temporaryFolder = Windows::Storage::ApplicationData::Current->TemporaryFolder;

		//InputTextBlock1->Text = "Installed location:\n" + installedLocation->Path + "\n" + "Local folder:\n" + localFolder->Path;
		//InputTextBlock1->Text = temporaryFolder->Path;
	}
	
	if (FAILED(hr))
	{
		throw Platform::Exception::CreateException(hr);
	}

	imagesSelected = 0UL;
	imagesAnalysed = 0UL;
	imagesToBeRotated = 0UL;
	imagesRotated = 0UL;
	imagesErrored = 0UL;
}

Scenario_AfterPick::~Scenario_AfterPick()
{
	SafeRelease(&pIWICImagingFactory);

	if (CoInitializeExSucceeded)
	{
		CoUninitialize();
	}
}

// <summary>
// Invoked when this page is about to be displayed in a Frame.
// </summary>
// <param name="e">Event data that describes how this page was reached. The Parameter
// property is typically used to configure the page.</param>
void Scenario_AfterPick::OnNavigatedTo(NavigationEventArgs^ e)
{
    // A pointer back to the main page.  This is needed if you want to call methods in MainPage such
    // as NotifyUser()
    rootPage = MainPage::Current;

	auto cancellationToken = ((concurrency::cancellation_token_source*)(rootPage->cts->Value))->get_token();

	storeData = ref new Data();

	GridView1->ItemsSource = storeData->Items;

	auto openPicker = ref new Windows::Storage::Pickers::FileOpenPicker();
	openPicker->SuggestedStartLocation = Windows::Storage::Pickers::PickerLocationId::PicturesLibrary;
	openPicker->ViewMode = Windows::Storage::Pickers::PickerViewMode::Thumbnail;
	openPicker->CommitButtonText = _resourceLoader->GetString("commitButtonText");

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
	auto pickerTask = concurrency::create_task(openPicker->PickMultipleFilesAsync());

	// Accept the unwrapped return value of previous call as input param
	pickerTask.then([this, cancellationToken](IVectorView<Windows::Storage::StorageFile^>^ files)
	{
		imagesSelected = static_cast<unsigned long>(files->Size);

		// file is null if user cancels the file picker.
		if (0 == imagesSelected)
		{
			// Stop work and clean up.
			concurrency::cancel_current_task();
		}

		rootPage->FlipButton();

		rootPage->SpinLogo_Start();

		InputTextBlock1->Text = _resourceLoader->GetString("initialising");		

		concurrency::create_task([this]
		{
			unsigned long imagesAnalysedCurrent = 0UL;
			unsigned long imagesAnalysedLast = 0UL;

			while (imagesAnalysed.load() <= imagesSelected.load())
			{
				concurrency::interruption_point();

				imagesAnalysedCurrent = imagesAnalysed.load();

				// Only dispatch a message if the value has changed
				if (imagesAnalysedCurrent != imagesAnalysedLast)
				{
					imagesAnalysedLast = imagesAnalysedCurrent;

					_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High,
						ref new Windows::UI::Core::DispatchedHandler([this, imagesAnalysedCurrent]()
					{
						Platform::String^ string;

						if (imagesAnalysedCurrent > 1UL)
						{
							string = _resourceLoader->GetString("analysedMany");

							// Subtract one length of "%lu", add 1U for null terminator
							unsigned int stringLength = string->Length() + imagesAnalysedCurrent.ToString()->Length() - 3U + 1U;

							wchar_t* wstring = new wchar_t[stringLength];

							swprintf_s(wstring, stringLength, string->Data(), imagesAnalysedCurrent);

							string = ref new Platform::String(wstring);

							delete[] wstring;
						}
						else
						{
							string = _resourceLoader->GetString("analysedOne");
						}

						InputTextBlock1->Text = string;
					}));
				}

				if (imagesAnalysedCurrent == imagesSelected.load())
				{
					break;
				}

				Sleep(20);
			}

			return;
		}, cancellationToken)
			.then([this]()
		{
			// as soon as you've analysed all the images, enable selections in the grid
			_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High,
				ref new Windows::UI::Core::DispatchedHandler([this]()
			{
				GridView1->SelectionMode = Windows::UI::Xaml::Controls::ListViewSelectionMode::Extended;
			}));

			unsigned long imagesRotatedCurrent = 0UL;
			unsigned long imagesRotatedLast = 0UL;
			unsigned long imagesToBeRotatedCurrent = 0UL;
			unsigned long imagesToBeRotatedLast = 0UL;
			unsigned long imagesErroredCurrent = 0UL;
			unsigned long imagesErroredLast = 0UL;

			while (imagesRotated.load() + imagesErrored.load() <= imagesToBeRotated.load())
			{
				concurrency::interruption_point();

				imagesRotatedCurrent = imagesRotated.load();
				imagesToBeRotatedCurrent = imagesToBeRotated.load();
				imagesErroredCurrent = imagesErrored.load();

				// Only dispatch a message if any of the values have changed
				if (imagesRotatedCurrent != imagesRotatedLast || imagesToBeRotatedCurrent != imagesToBeRotatedLast || imagesErroredCurrent != imagesErroredLast)
				{
					imagesRotatedLast = imagesRotatedCurrent;
					imagesToBeRotatedLast = imagesToBeRotatedCurrent;
					imagesErroredLast = imagesErroredCurrent;

					_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Normal,
						ref new Windows::UI::Core::DispatchedHandler([this, imagesRotatedCurrent, imagesToBeRotatedCurrent, imagesErroredCurrent]()
					{
						Platform::String^ string;
						
						if (imagesToBeRotatedCurrent > 1UL || (1UL == imagesErroredCurrent && 1UL == imagesToBeRotatedCurrent))
						{
							string = _resourceLoader->GetString("processedMany");

							// Subtract two lengths of "%lu", add 1U for null terminator
							unsigned int stringLength = string->Length() + imagesRotatedCurrent.ToString()->Length() + imagesToBeRotatedCurrent.ToString()->Length() - 2 * 3U + 1U;

							wchar_t* wstring = new wchar_t[stringLength];

							swprintf_s(wstring, stringLength, string->Data(), imagesRotatedCurrent, imagesToBeRotatedCurrent);

							string = ref new Platform::String(wstring);

							delete[] wstring;
						}
						else
						{
							string = _resourceLoader->GetString("processedOne");
						}						

						InputTextBlock1->Text = string;
					}));
				}

				if (imagesRotatedCurrent + imagesErroredCurrent == imagesToBeRotatedCurrent)
				{
					break;
				}

				Sleep(20);
			}

			// this must run at a lower priority than the RunAsync directly above so that all the progress messages are handled before this final one
			_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
				ref new Windows::UI::Core::DispatchedHandler([this]()
			{
				Platform::String^ string;

				if (0UL == imagesToBeRotated.load())
				{
					string = _resourceLoader->GetString("noImagesToBeRotated");

					InputTextBlock1->Text = string;
				}
				else
				{
					// if you've rotated all images without errors
					if (imagesRotated.load() == imagesToBeRotated.load())
					{
						if (imagesRotated.load() > 1UL)
						{
							string = _resourceLoader->GetString("processedAllMany");

							// Subtract one length of "%lu", add 1U for null terminator
							unsigned int stringLength = string->Length() + imagesRotated.load().ToString()->Length() - 3U + 1U;

							wchar_t* wstring = new wchar_t[stringLength];

							swprintf_s(wstring, stringLength, string->Data(), imagesRotated.load());

							string = ref new Platform::String(wstring);

							delete[] wstring;
						}
						else
						{
							string = _resourceLoader->GetString("processedAllOne");
						}

						InputTextBlock1->Text = string;
					}
				}

				rootPage->FlipButton();

				rootPage->SpinLogo_Stop();
			}));
		},
			cancellationToken,
			concurrency::task_continuation_context::use_arbitrary()
			);

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
							throw e;
						}
					}
				}

				concurrency::interruption_point();

				if (!item->Error->IsEmpty())
				{
					imagesErrored++;

					continue;
				}

				auto createReorientedTempFileAsyncTask = CreateReorientedTempFileAsync(item, rootPage->CropChecked, rootPage->ProgressiveChecked);

				createReorientedTempFileAsyncTask.then([this, cancellationToken, item](HRESULT hr)
				{
					concurrency::interruption_point();

					if SUCCEEDED(hr)
					{
						hr = FixMetadata(item, pIWICImagingFactory);

						if (SUCCEEDED(hr))
						{
							if (rootPage->DeleteThumbnails)
							{
								// Ignore return value
								DeleteJPEGThumbnailData(item, pIWICImagingFactory);
							}

							_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High,
								ref new Windows::UI::Core::DispatchedHandler([this, item]()
							{
								item->Orientation = 1U;
								item->OrientationXMP = 1U;
							}));

							concurrency::interruption_point();

							//Sleep((((rand() % 100) + 1) / 100.0) * 1000.0);

							auto getFileFromPathAsyncTask = concurrency::create_task(Windows::Storage::StorageFile::GetFileFromPathAsync(item->TempFilePath));

							getFileFromPathAsyncTask.then([this, cancellationToken, item](Windows::Storage::StorageFile^ tempFile)
							{
								concurrency::interruption_point();

								// Using CopyAndReplaceAsync & DeleteAsync instead of only MoveAndReplaceAsync to preserve the Created date metadata
								auto moveAndReplaceAsyncTask = concurrency::create_task(tempFile->CopyAndReplaceAsync(item->StorageFile));

								moveAndReplaceAsyncTask.then([this, item, tempFile]()
								{
									imagesRotated++;

									concurrency::create_task(tempFile->DeleteAsync(Windows::Storage::StorageDeleteOption::PermanentDelete));

									concurrency::interruption_point();

									// Rotate the preview
									_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
										ref new Windows::UI::Core::DispatchedHandler([this, item]()
									{
										GridView1->SelectedItems->Append(item);

										Windows::UI::Xaml::Controls::Primitives::SelectorItem^ sI = safe_cast<Windows::UI::Xaml::Controls::Primitives::SelectorItem^>(GridView1->ContainerFromItem(item));

										// this can return a nullptr when the item has not yet been rendered to the grid - this is normal!
										// when the item is due to be rendered, the ShowError() will get called on it anyway
										if (nullptr != sI)
										{
											ItemViewer^ iv = safe_cast<ItemViewer^>(sI->ContentTemplateRoot);

											if (nullptr != iv)
											{
												iv->ShowImage();
											}
										}
									}));
								}, cancellationToken);
							}, cancellationToken);
						}
						// If cannot fix the metadata
						else
						{
							imagesErrored++;

							_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
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

						_dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::Low,
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

		for (unsigned int i = 0U; i < files->Size; ++i)
		{
			concurrency::interruption_point();

			// Return the IRandomAccessStream^ object
			auto openingTask = concurrency::create_task(files->GetAt(i)->OpenAsync(Windows::Storage::FileAccessMode::Read));

			openingTask.then([this, cancellationToken, files, i](Windows::Storage::Streams::IRandomAccessStream^ fileStream)
			{
				concurrency::interruption_point();

				Microsoft::WRL::ComPtr<IStream> pIStream;

				HRESULT hr = CreateStreamOverRandomAccessStream(
					reinterpret_cast<IUnknown*>(fileStream),
					IID_PPV_ARGS(&pIStream)
					);

				if (SUCCEEDED(hr))
				{
					concurrency::interruption_point();

					Item^ item = ref new Item();

					hr = GetMetadata(pIStream.Get(), item, pIWICImagingFactory);

					imagesAnalysed++;

					if (SUCCEEDED(hr))
					{
						concurrency::interruption_point();

						if ((item->Orientation >= 2U && item->Orientation <= 8U)
							|| (item->OrientationXMP >= 2U && item->OrientationXMP <= 8U))
						{
							imagesToBeRotated++;

							auto getThumbnailTask = concurrency::create_task(files->GetAt(i)->GetThumbnailAsync(Windows::Storage::FileProperties::ThumbnailMode::SingleItem, 192U));

							getThumbnailTask.then([this, cancellationToken, files, i, item](Windows::Storage::FileProperties::StorageItemThumbnail^ thumbnail)
							{
								concurrency::interruption_point();

								// Set the stream as source of the bitmap
								Windows::UI::Xaml::Media::Imaging::BitmapImage^ bitmapImage = ref new Windows::UI::Xaml::Media::Imaging::BitmapImage();

								bitmapImage->SetSource(thumbnail);

								item->StorageFile = files->GetAt(i);

								item->UUID = GetUUID();

								// Add picked file to MostRecentlyUsedList
								item->MRUToken = Windows::Storage::AccessCache::StorageApplicationPermissions::MostRecentlyUsedList->Add(files->GetAt(i));

								item->Title = files->GetAt(i)->DisplayName;

								if (Windows::Storage::FileAttributes::ReadOnly == (files->GetAt(i)->Attributes & Windows::Storage::FileAttributes::ReadOnly))
								{
									item->Error = _resourceLoader->GetString("imageReadOnly");
								}

								// Set the bitmap as source of the Image control
								item->Image = bitmapImage;

								storeData->Items->Append(item);
							}, cancellationToken);
						}
					}
				}
			}, cancellationToken);
		}
	}, cancellationToken);
}

// <summary>
// We will visualize the data item in asynchronously in multiple phases for improved panning user experience 
// of large lists.  In this sample scneario, we will visualize different parts of the data item
// in the following order:
// 
//     1) Placeholders (visualized synchronously - Phase 0)
//     2) Title (visualized asynchronously - Phase 1)
//     3) Category and Image (visualized asynchronously - Phase 2)
//
// </summary>
// <param name="sender"></param>
// <param name="args"></param>
void JPG_Spinner::Scenario_AfterPick::ItemGridView_ContainerContentChanging(
    ListViewBase^ sender,
    Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs^ args)
{
    ItemViewer^ iv = safe_cast<ItemViewer^>(args->ItemContainer->ContentTemplateRoot);

    if (iv != nullptr)
    {
        // if the container is being added to the recycle queue (meaning it will not particiapte in 
        // visualizing data items for the time being), we clear out the data item
        if (args->InRecycleQueue == true)
        {
            iv->ClearData();
        }
        else if (args->Phase == 0)
        {
            iv->ShowPlaceholder(safe_cast<Item^>(args->Item));

            // Register for async callback to visualize Title asynchronously
            args->RegisterUpdateCallback(ContainerContentChangingDelegate);
        }
        else if (args->Phase == 1)
        {
            iv->ShowTitle();

            args->RegisterUpdateCallback(ContainerContentChangingDelegate);
        }
        else if (args->Phase == 2)
        {
            iv->ShowError();
            iv->ShowImage();
        }

        args->Handled = true;
    }
}
