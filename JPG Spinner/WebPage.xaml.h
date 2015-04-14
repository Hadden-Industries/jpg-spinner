//
// WebPage.xaml.h
// Declaration of the WebPage class
//

#pragma once

#include "WebPage.g.h"

namespace JPG_Spinner
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class WebPage sealed
	{
	public:
		WebPage();

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
	};
}
