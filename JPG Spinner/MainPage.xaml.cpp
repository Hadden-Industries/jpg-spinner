//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace JPG_Spinner;

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

// The Blank Page item template is documented at http://go.microsoft.com/fwlink/?LinkId=234238

MainPage^ MainPage::Current = nullptr;

MainPage::MainPage()
{
	InitializeComponent();

	// This is a static public property that allows downstream pages to get a handle to the MainPage instance
	// in order to call methods that are in this class.
	MainPage::Current = this;

	buttonIsSelectFiles = true;

	DeleteThumbnails = true;
}

void JPG_Spinner::MainPage::ShowDebugText(Platform::String^ debugText)
{
	if (Windows::UI::Xaml::Visibility::Collapsed == DebugBorder->Visibility)
	{
		DebugBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	DebugText->Text = debugText;
}

void JPG_Spinner::MainPage::Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (buttonIsSelectFiles)
	{
		CropChecked = CheckBoxCrop->IsChecked->Value;

		ProgressiveChecked = CheckBoxProgressive->IsChecked->Value;

		_cts = concurrency::cancellation_token_source();

		Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.Scenario_AfterPick", Windows::UI::Xaml::Interop::TypeKind::Custom };

		ScenarioFrame->Navigate(scenarioType, this);
	}
	else
	{
		_cts.cancel();

		SpinLogo_Stop();

		FlipButton();
	}
}

void JPG_Spinner::MainPage::TextBlockCrop_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (buttonIsSelectFiles)
	{
		Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.ExplanationCrop", Windows::UI::Xaml::Interop::TypeKind::Custom };

		ScenarioFrame->Navigate(scenarioType, this);
	}
}

void JPG_Spinner::MainPage::TextBlockProgressive_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (buttonIsSelectFiles)
	{
		Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.ExplanationProgressive", Windows::UI::Xaml::Interop::TypeKind::Custom };

		ScenarioFrame->Navigate(scenarioType, this);
	}
}

void JPG_Spinner::MainPage::HyperLinkButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	if (buttonIsSelectFiles)
	{
		Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.WebPage", Windows::UI::Xaml::Interop::TypeKind::Custom };

		ScenarioFrame->Navigate(scenarioType, sender);
	}
}

void JPG_Spinner::MainPage::SpinLogo_Start()
{
	SpinLogo->Stop();

	SpinLogoAnimation->RepeatBehavior = Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::Forever;

	SpinLogo->Begin();
}

void JPG_Spinner::MainPage::SpinLogo_Stop()
{
	SpinLogoAnimation->RepeatBehavior = Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::FromCount(1.0);
}

void JPG_Spinner::MainPage::ButtonSelectFiles_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	ButtonSelectFiles->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

void JPG_Spinner::MainPage::FlipButton()
{
	if (buttonIsSelectFiles)
	{
		ButtonSelectFiles->Content = Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView()->GetString("cancel"); 
	}
	else
	{
		ButtonSelectFiles->Content = Windows::ApplicationModel::Resources::ResourceLoader::GetForCurrentView()->GetString("selectFiles");
	}

	buttonIsSelectFiles = !buttonIsSelectFiles;
}

concurrency::task<bool> JPG_Spinner::MainPage::SaveSettingAsync(Platform::String^ key, Platform::String^ value)
{
	return concurrency::create_task(Windows::System::UserProfile::UserInformation::GetDisplayNameAsync())
		.then([this, key, value](Platform::String^ displayName)
	{
		Windows::Storage::ApplicationDataContainer^ localSettings = Windows::Storage::ApplicationData::Current->LocalSettings;

		if (nullptr != displayName)
		{
			Windows::Storage::ApplicationDataContainer^ container;

			// if there is a container with the user's name
			if (localSettings->Containers->HasKey(displayName))
			{
				// then retrieve it
				container = localSettings->Containers->Lookup(displayName);
			}
			else
			{
				// else create one
				container = localSettings->CreateContainer(displayName, Windows::Storage::ApplicationDataCreateDisposition::Always);
			}

			return container->Values->Insert(key, value);
		}
		else
		{
			// insert for all users
			return localSettings->Values->Insert(key, value);
		}
	});
}

concurrency::task<Platform::String^> JPG_Spinner::MainPage::LoadSettingAsync(Platform::String^ key)
{
	return concurrency::create_task(Windows::System::UserProfile::UserInformation::GetDisplayNameAsync())
		.then([this, key](Platform::String^ displayName)
	{
		Platform::String^ value = nullptr;

		Windows::Storage::ApplicationDataContainer^ localSettings = Windows::Storage::ApplicationData::Current->LocalSettings;

		if (nullptr != displayName)
		{
			Windows::Storage::ApplicationDataContainer^ container;

			// if there is a container with the user's name
			if (localSettings->Containers->HasKey(displayName))
			{
				// then retrieve it
				container = localSettings->Containers->Lookup(displayName);
			}
			else
			{
				return value;
			}

			return safe_cast<Platform::String^>(container->Values->Lookup(key));
		}
		else
		{
			if (localSettings->Values->HasKey(key))
			{
				return safe_cast<Platform::String^>(localSettings->Values->Lookup(key));
			}
			else
			{
				return value;
			}
		}
	});
}

void JPG_Spinner::MainPage::CheckBoxProgressive_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	concurrency::create_task(SaveSettingAsync("CheckBoxProgressive", Boolean(true).ToString()));
}

void JPG_Spinner::MainPage::CheckBoxProgressive_Unchecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	concurrency::create_task(SaveSettingAsync("CheckBoxProgressive", Boolean(false).ToString()));
}

void JPG_Spinner::MainPage::CheckBoxCrop_Checked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	concurrency::create_task(SaveSettingAsync("CheckBoxCrop", Boolean(true).ToString()));
}

void JPG_Spinner::MainPage::CheckBoxCrop_Unchecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	concurrency::create_task(SaveSettingAsync("CheckBoxCrop", Boolean(false).ToString()));
}

void JPG_Spinner::MainPage::CheckBoxProgressive_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	concurrency::create_task(LoadSettingAsync("CheckBoxProgressive"))
		.then([this](Platform::String^ value)
	{
		if (nullptr != value)
		{
			if (Boolean(true).ToString() == value)
			{
				CheckBoxProgressive->IsChecked = true;
			}
			else
			{
				CheckBoxProgressive->IsChecked = false;
			}
		}
		else
		{
			CheckBoxProgressive->IsChecked = true;
		}
	});
}

void JPG_Spinner::MainPage::CheckBoxCrop_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	concurrency::create_task(LoadSettingAsync("CheckBoxCrop"))
		.then([this](Platform::String^ value)
	{
		if (nullptr != value)
		{
			if (Boolean(true).ToString() == value)
			{
				CheckBoxCrop->IsChecked = true;
			}
			else
			{
				CheckBoxCrop->IsChecked = false;
			}
		}
		else
		{
			CheckBoxCrop->IsChecked = false;
		}
	});
}

void JPG_Spinner::MainPage::Logo_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (buttonIsSelectFiles && Windows::UI::Xaml::Media::Animation::ClockState::Active != SpinLogo->GetCurrentState())
	{
		if (Windows::UI::Xaml::Media::Animation::ClockState::Filling == SpinLogo->GetCurrentState())
		{
			SpinLogo->Stop();
		}

		SpinLogo->Begin();
	}
}