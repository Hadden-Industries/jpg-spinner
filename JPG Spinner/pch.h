//
// pch.h
// Header for standard system include files.
//

#pragma once

#define MAX_PATH_UNICODE 32767

#include <collection.h>
#include <ppltasks.h>

#include "App.xaml.h"
#include <wincodec.h> // IWICImagingFactory
#include <Shcore.h> // CreateStreamOverRandomAccessStream
#include <io.h> // _open_osfhandle
#include <Fcntl.h> // _O_APPEND _O_RDONLY
#include <robuffer.h> // IBufferByteAccess
#include <sstream> // wostringstream
#include <d2d1_1.h> // D2D1::Matrix3x2F
#include <Wincodecsdk.h> // IWICMetadataBlockWriter
#include "jpeglib.h" // jpeg_stdio_src

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

class OrientationHelper
{
	public:
		OrientationHelper::OrientationHelper(unsigned char orientation, unsigned short xOffset = 0U, unsigned short yOffset = 0U);

		Windows::UI::Xaml::Media::Matrix getMatrix();

		Windows::UI::Xaml::Media::Matrix getInverseMatrix();

		bool XYFlips();

		WICBitmapTransformOptions GetWICBitmapTransformOptions();

	private:
		Windows::UI::Xaml::Media::Matrix _Matrix;
		bool _XYFlips;
		int _WICBitmapTransformOptions;
};