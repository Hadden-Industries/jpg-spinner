#pragma once

#include <windows.h>

// WinBase.h exposes GetCurrentTime as a function-like macro, while the WinUI
// projection declares ABI methods with that name and parameters.  Preserve the
// Win32 macro for later consumers, but hide it while C++/WinRT parses WinUI.
// This is the scoped collision remedy documented in Microsoft's C++/WinRT FAQ.
#pragma push_macro("GetCurrentTime")
#undef GetCurrentTime
#include <winrt/Microsoft.UI.Xaml.h>
#pragma pop_macro("GetCurrentTime")

#include <winrt/Windows.Foundation.h>
