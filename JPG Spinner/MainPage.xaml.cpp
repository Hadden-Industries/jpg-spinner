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
}

void JPG_Spinner::MainPage::Button_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	TrimChecked = CheckBoxTrim->IsChecked->Value;

	ProgressiveChecked = CheckBoxProgressive->IsChecked->Value;

	Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.Scenario_AfterPick", Windows::UI::Xaml::Interop::TypeKind::Custom };

	ScenarioFrame->Navigate(scenarioType, this);
}


void JPG_Spinner::MainPage::TextBlockTrim_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (Windows::UI::Xaml::Visibility::Collapsed == DebugBorder->Visibility)
	{
		DebugBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	DebugText->Text = "Clicked Trim";
}


void JPG_Spinner::MainPage::TextBlockProgressive_PointerReleased(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e)
{
	if (Windows::UI::Xaml::Visibility::Collapsed == DebugBorder->Visibility)
	{
		DebugBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	DebugText->Text = "Clicked Progressive";
}

void JPG_Spinner::MainPage::HyperLinkButton_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	Windows::UI::Xaml::Interop::TypeName scenarioType = { L"JPG_Spinner.WebPage", Windows::UI::Xaml::Interop::TypeKind::Custom };

	ScenarioFrame->Navigate(scenarioType, sender);
}

void JPG_Spinner::MainPage::spinme_Start()
{
	spinrect->Stop();

	doubleAnimation->RepeatBehavior = Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::Forever;

	/*if (Windows::UI::Xaml::Visibility::Collapsed == DebugBorder->Visibility)
	{
		DebugBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	DebugText->Text = "Changed RepeatBehavior to Forever";*/

	spinrect->Begin();
}

void JPG_Spinner::MainPage::spinme_Stop()
{
	doubleAnimation->RepeatBehavior = Windows::UI::Xaml::Media::Animation::RepeatBehaviorHelper::FromCount(1.0);

	/*if (Windows::UI::Xaml::Visibility::Collapsed == DebugBorder->Visibility)
	{
		DebugBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	DebugText->Text = "Changed RepeatBehavior to FromCount(1.0)";*/
}

void JPG_Spinner::MainPage::ButtonPickFiles_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	ButtonPickFiles->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}
