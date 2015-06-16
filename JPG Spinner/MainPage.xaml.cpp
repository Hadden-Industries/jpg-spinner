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

JPG_Spinner::MainPage^ MainPage::Current = nullptr;

JPG_Spinner::MainPage::MainPage()
{
	InitializeComponent();

	// This is a static public property that allows downstream pages to get a handle to the MainPage instance
	// in order to call methods that are in this class.
	MainPage::Current = this;

	buttonIsSelectFiles = true;

	//DeleteThumbnails = false;
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
		_cts = concurrency::cancellation_token_source();

		Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.Scenario_AfterPick", Windows::UI::Xaml::Interop::TypeKind::Custom };

		ScenarioFrame->Navigate(scenarioType, this);
	}
	else
	{
		CancelProcessing();
	}
}

void JPG_Spinner::MainPage::CancelProcessing()
{
	if (!buttonIsSelectFiles)
	{
		_cts.cancel();

		SpinLogo_Stop();

		FlipButton();
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

void JPG_Spinner::MainPage::LoadWebPage(Platform::String^ uri)
{
	if (buttonIsSelectFiles)
	{
		Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.WebPage", Windows::UI::Xaml::Interop::TypeKind::Custom };

		auto hyperlinkButton = ref new HyperlinkButton();

		hyperlinkButton->Tag = uri;

		ScenarioFrame->Navigate(scenarioType, hyperlinkButton);
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
	if (Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::FromCount(1.0) != SpinLogoAnimation->RepeatBehavior)
	{
		SpinLogoAnimation->RepeatBehavior = Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::FromCount(1.0);
	}
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

void JPG_Spinner::MainPage::Navigate(Windows::UI::Xaml::Interop::TypeName type, Platform::Object^ obj)
{
	if (buttonIsSelectFiles)
	{
		ScenarioFrame->Navigate(type, obj);
	}
}