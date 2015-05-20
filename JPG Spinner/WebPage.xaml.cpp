//
// WebPage.xaml.cpp
// Implementation of the WebPage class
//

#include "pch.h"
#include "WebPage.xaml.h"

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

WebPage::WebPage()
{
	InitializeComponent();
}

void WebPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	auto uri = ref new Uri(safe_cast<String^>(safe_cast<HyperlinkButton^>(e->Parameter)->Tag));

	WebView1->Navigate(uri);
}