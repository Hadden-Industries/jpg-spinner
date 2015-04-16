//
// ItemViewer.xaml.h
// Declaration of the ItemViewer class
//

#pragma once

#include "ItemViewer.g.h"

namespace JPG_Spinner
{
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class ItemViewer sealed
	{
	public:
		ItemViewer();
		void ShowPlaceholder(Item^ item);
		void ShowTitle();
		void ShowError();
		void ShowImage();
		void ClearData();
		void RotateImage(double angle);

	private:
		// reference to the data item that will be visualized by this ItemViewer
		Item^ _item;

	};
}