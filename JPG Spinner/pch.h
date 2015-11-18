//
// pch.h
// Header for standard system include files.
//

#pragma once

#include <collection.h>
#include <ppltasks.h>

#include "App.xaml.h"
#include <wincodec.h> // IWICImagingFactory
#include <Shcore.h> // CreateStreamOverRandomAccessStream
#include <io.h> // _open_osfhandle
#include <Fcntl.h> // _O_APPEND _O_RDONLY
#include <robuffer.h> // IBufferByteAccess
#include <sstream> // wostringstream
#include <Wincodecsdk.h> // IWICMetadataBlockWriter
#include "jpeglib.h" // jpeg_stdio_src
#include "OrientationHelper.h" // OrientationHelper
#include <iomanip> // std::get_time

extern "C" {
#include "transupp.h" // Support routines for jpegtran
}

template<class Interface>
inline void SafeRelease(Interface **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease)
	{
		(*ppInterfaceToRelease)->Release();
		*ppInterfaceToRelease = nullptr;
	}
}

Platform::String^ HResultToHexString(HRESULT hr);

extern Platform::String^ applicationInsightsKey;