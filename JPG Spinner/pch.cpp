//
// pch.cpp
// Include the standard header and generate the precompiled header.
//

#include "pch.h"

Platform::String^ HResultToHexString(HRESULT hr)
{
	std::wostringstream os;

	os << L"0x";

	os << std::hex << std::uppercase << hr;

	return ref new Platform::String(os.str().data());
}