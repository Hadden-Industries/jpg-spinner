//
// App.xaml.cpp
// Implementation of the App class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "SettingsFlyout.xaml.h"
#include "SettingsPrivacyPolicy.xaml.h"

using namespace JPG_Spinner;

using namespace Platform;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

// The Blank Application template is documented at http://go.microsoft.com/fwlink/?LinkId=234227

/// <summary>
/// Initializes the singleton application object.  This is the first line of authored code
/// executed, and as such is the logical equivalent of main() or WinMain().
/// </summary>
App::App()
{
	InitializeComponent();

	Suspending += ref new SuspendingEventHandler(this, &App::OnSuspending);
}

/// <summary>
/// Invoked when the application is launched normally by the end user.	Other entry points
/// will be used such as when the application is launched to open a specific file.
/// </summary>
/// <param name="e">Details about the launch request and process.</param>
void App::OnLaunched(Windows::ApplicationModel::Activation::LaunchActivatedEventArgs^ e)
{

#if _DEBUG
	// Show graphics profiling information while debugging.
	if (IsDebuggerPresent())
	{
		// Display the current frame rate counters
		//DebugSettings->EnableFrameRateCounter = true;
		//DebugSettings->EnableRedrawRegions = true;
		//DebugSettings->IsOverdrawHeatMapEnabled = true;
	}
#endif

	auto rootFrame = dynamic_cast<Frame^>(Window::Current->Content);

	// Do not repeat app initialization when the Window already has content,
	// just ensure that the window is active
	if (rootFrame == nullptr)
	{
		// Create a Frame to act as the navigation context and associate it with
		// a SuspensionManager key
		rootFrame = ref new Frame();

		// Set the default language
		rootFrame->Language = Windows::Globalization::ApplicationLanguages::Languages->GetAt(0U);

		rootFrame->NavigationFailed += ref new Windows::UI::Xaml::Navigation::NavigationFailedEventHandler(this, &App::OnNavigationFailed);

		if (e->PreviousExecutionState == ApplicationExecutionState::Terminated)
		{
			// TODO: Restore the saved session state only when appropriate, scheduling the
			// final launch steps after the restore is complete

		}

		if (rootFrame->Content == nullptr)
		{
			// When the navigation stack isn't restored navigate to the first page,
			// configuring the new page by passing required information as a navigation
			// parameter
			rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
		}
		// Place the frame in the current Window
		Window::Current->Content = rootFrame;
		// Ensure the current window is active
		Window::Current->Activate();
	}
	else
	{
		if (rootFrame->Content == nullptr)
		{
			// When the navigation stack isn't restored navigate to the first page,
			// configuring the new page by passing required information as a navigation
			// parameter
			rootFrame->Navigate(TypeName(MainPage::typeid), e->Arguments);
		}
		// Ensure the current window is active
		Window::Current->Activate();
	}

	// Get the files in the temporary folder
	Concurrency::create_task(Windows::Storage::ApplicationData::Current->TemporaryFolder->GetFilesAsync())
		.then([](IVectorView<Windows::Storage::StorageFile^>^ filesInFolder)
	{
		// Iterate over the files
		for (auto it = filesInFolder->First(); it->HasCurrent; it->MoveNext())
		{
			auto file = it->Current;

			// Permanently delete the file
			Concurrency::create_task(file->DeleteAsync(Windows::Storage::StorageDeleteOption::PermanentDelete));
		}
	});

	IPropertyValue^ value = LoadSetting("numberLogicalProcessorsToUse");
	SYSTEM_INFO systemInfo = SYSTEM_INFO();

	GetNativeSystemInfo(&systemInfo);

	// If you have not saved this setting before
	if (nullptr == value
		// or the saved value is not of the correct type
		|| value->Type != PropertyType::UInt32
		// or the saved value is greater than the total current number of processors
		|| value->GetUInt32() > static_cast<uint32_t>(systemInfo.dwNumberOfProcessors))
	{
		// To somewhat account for HyperThreading and to reduce occurrence of alloc errors within JPEG library
		const float reductionFactor = 2.0f;

		uint32_t numberLogicalProcessorsToUse = static_cast<uint32_t>(static_cast<float>(systemInfo.dwNumberOfProcessors) / reductionFactor);

		// Sanity check
		if (0U == numberLogicalProcessorsToUse)
		{
			numberLogicalProcessorsToUse = 1U;
		}

		SaveSetting("numberLogicalProcessorsToUse", PropertyValue::CreateUInt32(numberLogicalProcessorsToUse));
	}

	value = LoadSetting("megabytesRAMToUse");

	uint64_t megabytesRAMToUse = (MAX_MEM_FOR_ALL_JPEGS) / (1024ULL * 1024ULL);

	// If you have not saved this setting before
	if (nullptr == value
		// or the saved value is not of the correct type
		|| value->Type != PropertyType::UInt64
		// or the saved value is more than the current allowable
		|| value->GetUInt64() > megabytesRAMToUse)
	{
		SaveSetting("megabytesRAMToUse", PropertyValue::CreateUInt64(megabytesRAMToUse));
	}

	value = LoadSetting("CheckBoxProgressive");

	// If you have not saved this setting before
	if (nullptr == value
		// or the saved value is not of the correct type
		|| value->Type != PropertyType::Boolean)
	{
		SaveSetting("CheckBoxProgressive", PropertyValue::CreateBoolean(true));
	}

	value = LoadSetting("CheckBoxCrop");

	// If you have not saved this setting before
	if (nullptr == value
		// or the saved value is not of the correct type
		|| value->Type != PropertyType::Boolean)
	{
		SaveSetting("CheckBoxCrop", PropertyValue::CreateBoolean(false));
	}
}

/// <summary>
/// Invoked when application execution is being suspended.	Application state is saved
/// without knowing whether the application will be terminated or resumed with the contents
/// of memory still intact.
/// </summary>
/// <param name="sender">The source of the suspend request.</param>
/// <param name="e">Details about the suspend request.</param>
void App::OnSuspending(Object^ /*sender*/, SuspendingEventArgs^ e)
{
	auto suspendingDeferral = e->SuspendingOperation->GetDeferral();

	// Cancel all processing
	MainPage::Current->CancelProcessing();

	suspendingDeferral->Complete();
}

/// <summary>
/// Invoked when Navigation to a certain page fails
/// </summary>
/// <param name="sender">The Frame which failed navigation</param>
/// <param name="e">Details about the navigation failure</param>
void App::OnNavigationFailed(Platform::Object^ /*sender*/, Windows::UI::Xaml::Navigation::NavigationFailedEventArgs^ e)
{
	throw ref new FailureException("Failed to load Page " + e->SourcePageType.Name);
}

Item::Item() :
_StorageFile(nullptr),
_UUID(Platform::Guid(GUID_NULL)),
//_MRUToken(""),
_Error(""),
_Image(nullptr),
_Orientation(0U),
_OrientationXMP(0U),
//_JPEGInterchangeFormat(0U),
//_JPEGInterchangeFormatLength(0U),
_HasThumbnail(false)
{
	PropVariantInit(&_SubjectArea);
	PropVariantInit(&_SubjectLocation);
}

Windows::Foundation::IPropertyValue^ JPG_Spinner::LoadSetting(Platform::String^ key)
{
	Windows::Foundation::IPropertyValue^ value = nullptr;

	Windows::Storage::ApplicationDataContainer^ localSettings = Windows::Storage::ApplicationData::Current->LocalSettings;

	if (localSettings->Values->HasKey(key))
	{
		try
		{
			value = safe_cast<Windows::Foundation::IPropertyValue^>(localSettings->Values->Lookup(key));
		}
		catch (InvalidCastException^ e) {}
	}

	return value;
}

bool JPG_Spinner::SaveSetting(Platform::String^ key, Platform::Object^ value)
{
	return Windows::Storage::ApplicationData::Current->LocalSettings->Values->Insert(key, value);
}