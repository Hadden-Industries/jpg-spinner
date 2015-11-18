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
void JPG_Spinner::SettingsFlyout::SettingsFlyout_AcceleratorKeyActivated(CoreDispatcher^ /*sender*/, AcceleratorKeyEventArgs^ args)
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
			//this->Hide();
		}
	}
}

void JPG_Spinner::SettingsFlyout::SettingsFlyout_Loaded(Platform::Object^ /*sender*/, Windows::UI::Xaml::RoutedEventArgs^ /*e*/)
{
	// Register accelerator keys
	_acceleratorKeyEventToken = Window::Current->CoreWindow->Dispatcher->AcceleratorKeyActivated += ref new TypedEventHandler<CoreDispatcher^, AcceleratorKeyEventArgs^>(
		this,
		&SettingsFlyout::SettingsFlyout_AcceleratorKeyActivated);

	_navigationShortcutsRegistered = true;

	// Obtain values for the settings
	SYSTEM_INFO systemInfo = SYSTEM_INFO();

	GetNativeSystemInfo(&systemInfo);

	SliderProcessor->Maximum = static_cast<double>(systemInfo.dwNumberOfProcessors);

	SliderProcessor->Value = static_cast<double>(LoadSetting("numberLogicalProcessorsToUse")->GetUInt32());

	SliderMemory->Maximum = static_cast<double>((MAX_MEM_FOR_ALL_JPEGS) / (1024ULL * 1024ULL));

	// If the app was built for x64
	if (Windows::System::ProcessorArchitecture::X64 == Windows::ApplicationModel::Package::Current->Id->Architecture)
	{
		// then increase the step sizes
		SliderMemory->StepFrequency = 512.0;

		SliderMemory->TickFrequency = 1024.0;
	}

	SliderMemory->Value = static_cast<double>(LoadSetting("megabytesRAMToUse")->GetUInt64());
}

void JPG_Spinner::SettingsFlyout::SettingsFlyout_Unloaded(Platform::Object^ /*sender*/, Windows::UI::Xaml::RoutedEventArgs^ /*e*/)
{
	// Save the setting values
	SaveSetting(
		"numberLogicalProcessorsToUse",
		PropertyValue::CreateUInt32(
			static_cast<uint32_t>(SliderProcessor->Value)
			)
		);

	SaveSetting(
		"megabytesRAMToUse",
		PropertyValue::CreateUInt64(
			static_cast<uint64_t>(SliderMemory->Value)
			)
		);

	SaveSetting(
		"CheckBoxProgressive",
		PropertyValue::CreateBoolean(CheckBoxProgressive->IsOn)
		);

	SaveSetting(
		"CheckBoxCrop",
		PropertyValue::CreateBoolean(CheckBoxCrop->IsOn)
		);

	// Deregister accelerator keys 
	if (_navigationShortcutsRegistered)
	{
		Window::Current->CoreWindow->Dispatcher->AcceleratorKeyActivated -= _acceleratorKeyEventToken;

		_navigationShortcutsRegistered = false;
	}
}

void JPG_Spinner::SettingsFlyout::TextBlockCrop_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.ExplanationCrop", Windows::UI::Xaml::Interop::TypeKind::Custom };

	MainPage::Current->Navigate(scenarioType, this);
}

void JPG_Spinner::SettingsFlyout::TextBlockProgressive_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.ExplanationProgressive", Windows::UI::Xaml::Interop::TypeKind::Custom };

	MainPage::Current->Navigate(scenarioType, this);
}

void JPG_Spinner::SettingsFlyout::ToggleSwitch_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ /*e*/)
{
	IPropertyValue^ value = LoadSetting(safe_cast<ToggleSwitch^>(sender)->Name);

	// if the current value is not the stored
	if (safe_cast<ToggleSwitch^>(sender)->IsOn != value->GetBoolean())
	{
		// toggle
		safe_cast<ToggleSwitch^>(sender)->IsOn = !safe_cast<ToggleSwitch^>(sender)->IsOn;
	}
}