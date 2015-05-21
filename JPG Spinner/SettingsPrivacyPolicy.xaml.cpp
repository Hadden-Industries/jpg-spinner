//
// SettingsPrivacyPolicy.xaml.cpp
// Implementation of the SettingsPrivacyPolicy class
//

#include "pch.h"
#include "SettingsPrivacyPolicy.xaml.h"

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
using namespace Windows::UI::ApplicationSettings;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Core;
using namespace Windows::System;

// The SettingsFlyout item template is documented at http://go.microsoft.com/fwlink/?LinkId=273769

JPG_Spinner::SettingsPrivacyPolicy::SettingsPrivacyPolicy()
{
	InitializeComponent();

	this->_navigationShortcutsRegistered = false;
}

// Invoked on every keystroke, including system keys such as Alt key combinations, when this page is active and occupies the entire window.
// Used to detect keyboard back navigation via Alt+Left key combination.
//
void JPG_Spinner::SettingsPrivacyPolicy::SettingsPrivacyPolicy_AcceleratorKeyActivated(CoreDispatcher^ sender, AcceleratorKeyEventArgs^ args)
{
	// Only investigate further when Left is pressed
	if (args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown && args->VirtualKey == VirtualKey::Left)
	{
		auto coreWindow = Window::Current->CoreWindow;
		auto downState = Windows::UI::Core::CoreVirtualKeyStates::Down;

		// Check for modifier keys
		// The Menu VirtualKey signifies Alt
		bool menuKey = (coreWindow->GetKeyState(VirtualKey::Menu) & downState) == downState;
		bool controlKey = (coreWindow->GetKeyState(VirtualKey::Control) & downState) == downState;
		bool shiftKey = (coreWindow->GetKeyState(VirtualKey::Shift) & downState) == downState;

		if (menuKey && !controlKey && !shiftKey)
		{
			args->Handled = true;
			this->Hide();
		}
	}
}

void JPG_Spinner::SettingsPrivacyPolicy::SettingsPrivacyPolicy_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	(void)sender;
	(void)e;

	rootPage = MainPage::Current;

	// Register accelerator keys
	_acceleratorKeyEventToken = Window::Current->CoreWindow->Dispatcher->AcceleratorKeyActivated += ref new TypedEventHandler<CoreDispatcher^, AcceleratorKeyEventArgs^>(
		this,
		&SettingsPrivacyPolicy::SettingsPrivacyPolicy_AcceleratorKeyActivated);

	_navigationShortcutsRegistered = true;
}

void JPG_Spinner::SettingsPrivacyPolicy::SettingsPrivacyPolicy_Unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	(void)sender;
	(void)e;

	// Deregister accelerator keys 
	if (_navigationShortcutsRegistered)
	{
		Window::Current->CoreWindow->Dispatcher->AcceleratorKeyActivated -= _acceleratorKeyEventToken;

		_navigationShortcutsRegistered = false;
	}
}

void JPG_Spinner::SettingsPrivacyPolicy::PrivacyPolicyDate_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	// Extract the string from the XAML (in ISO 8601 YYYY-MM-DD format)
	std::wstring dateString = PrivacyPolicyDate->Text->Data();

	SYSTEMTIME systemTime = { 0 };

	systemTime.wYear = static_cast<WORD>(wcstoul(dateString.substr(0, 4).c_str(), nullptr, 10));
	systemTime.wMonth = static_cast<WORD>(wcstoul(dateString.substr(5, 2).c_str(), nullptr, 10));
	systemTime.wDay = static_cast<WORD>(wcstoul(dateString.substr(8, 2).c_str(), nullptr, 10));

	FILETIME fileTime = { 0 };

	if (SystemTimeToFileTime(&systemTime, &fileTime))
	{
		Windows::Foundation::DateTime dateTime;

		ULARGE_INTEGER uLarge_Integer = { 0 };

		uLarge_Integer.HighPart = fileTime.dwHighDateTime;
		uLarge_Integer.LowPart = fileTime.dwLowDateTime;

		dateTime.UniversalTime = static_cast<long long>(uLarge_Integer.QuadPart);

		Windows::Globalization::DateTimeFormatting::DateTimeFormatter^ dateTimeFormatter = Windows::Globalization::DateTimeFormatting::DateTimeFormatter::ShortDate::get();

		PrivacyPolicyDate->Text = dateTimeFormatter->Format(dateTime);
	}
}

void JPG_Spinner::SettingsPrivacyPolicy::HyperlinkPrivacyPolicy_Click(Windows::UI::Xaml::Documents::Hyperlink^ sender, Windows::UI::Xaml::Documents::HyperlinkClickEventArgs^ args)
{
	(void)args;

	Platform::String^ uri;
	
	if ("HyperlinkPrivacyPolicy" == sender->Name)
	{
		uri = "http://haddenindustries.com/legal.html#privacy-policy";
	}
	else if ("HyperlinkAnalyzingCrashReports" == sender->Name)
	{
		uri = "https://msdn.microsoft.com/en-gb/library/windows/apps/jj933262.aspx";
	}

	rootPage->LoadWebPage(uri);
}
