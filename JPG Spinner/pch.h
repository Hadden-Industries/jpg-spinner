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

extern "C" {
#include "jpeglib.h" // jpeg_stdio_src
#include "cdjpeg.h" // Common decls for cjpeg/djpeg applications
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