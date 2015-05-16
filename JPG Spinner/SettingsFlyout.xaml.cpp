//
// SettingsFlyout.xaml.cpp
// Implementation of the SettingsFlyout class
//

#include "pch.h"
#include "SettingsFlyout.xaml.h"

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

JPG_Spinner::SettingsFlyout::SettingsFlyout()
{
	InitializeComponent();

	this->_navigationShortcutsRegistered = false;
}

// Invoked on every keystroke, including system keys such as Alt key combinations, when this page is active and occupies the entire window.
// Used to detect keyboard back navigation via Alt+Left key combination.
//
void JPG_Spinner::SettingsFlyout::SettingsFlyout_AcceleratorKeyActivated(CoreDispatcher^ sender, AcceleratorKeyEventArgs^ args)
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

void JPG_Spinner::SettingsFlyout::SettingsFlyout_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	(void)sender;
	(void)e;

	// Register accelerator keys
	_acceleratorKeyEventToken = Window::Current->CoreWindow->Dispatcher->AcceleratorKeyActivated += ref new TypedEventHandler<CoreDispatcher^, AcceleratorKeyEventArgs^>(
		this,
		&SettingsFlyout::SettingsFlyout_AcceleratorKeyActivated);

	_navigationShortcutsRegistered = true;

	// Obtain values for the settings
	SYSTEM_INFO systemInfo = SYSTEM_INFO();

	GetNativeSystemInfo(&systemInfo);

	SliderProcessor->Maximum = static_cast<double>(systemInfo.dwNumberOfProcessors);

	concurrency::create_task(LoadSettingAsync("numberLogicalProcessorsToUse"))
		.then([this](Platform::String^ value)
	{
		if (nullptr != value)
		{
			SliderProcessor->Value = static_cast<double>(wcstoul(value->Data(), nullptr, 0));
		}
	});

	concurrency::create_task(LoadSettingAsync("megabytesRAMToUse"))
		.then([this](Platform::String^ value)
	{
		if (nullptr != value)
		{
			SliderMemory->Value = static_cast<double>(wcstoull(value->Data(), nullptr, 0));
		}
	});
}

void JPG_Spinner::SettingsFlyout::SettingsFlyout_Unloaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	(void)sender;
	(void)e;

	// Save the setting values
	concurrency::create_task(SaveSettingAsync("numberLogicalProcessorsToUse", static_cast<unsigned long>(SliderProcessor->Value).ToString()));

	concurrency::create_task(SaveSettingAsync("megabytesRAMToUse", static_cast<unsigned long long>(SliderMemory->Value).ToString()));

	// Deregister accelerator keys 
	if (_navigationShortcutsRegistered)
	{
		Window::Current->CoreWindow->Dispatcher->AcceleratorKeyActivated -= _acceleratorKeyEventToken;

		_navigationShortcutsRegistered = false;
	}
}