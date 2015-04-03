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
