//
// ItemViewer.xaml.cpp
// Implementation of the ItemViewer class
//

#include "pch.h"
#include "ItemViewer.xaml.h"

using namespace JPG_Spinner;
using namespace Windows::UI::Xaml::Controls;

ItemViewer::ItemViewer()
{
    InitializeComponent();
}

// <summary>
// This method visualizes the placeholder state of the data item. When 
// showing a placeholder, we set the opacity of other elements to zero
// so that stale data is not visible to the end user.  Note that we use
// Grid's background color as the placeholder background.
// </summary>
// <param name="item"></param>
void ItemViewer::ShowPlaceholder(Item^ item)
{
    _item = item;
    titleTextBlock->Opacity = 0;
    errorTextBlock->Opacity = 0;
    image->Opacity = 0;
}

// <summary>
// Visualize the Title by updating the TextBlock for Title and setting Opacity
// to 1.
// </summary>
void ItemViewer::ShowTitle()
{
    if (_item != nullptr)
    {
        titleTextBlock->Text = _item->Title;
    }
    titleTextBlock->Opacity = 1;
}

// <summary>
// Visualize category information by updating the correct TextBlock and 
// setting Opacity to 1.
// </summary>
void ItemViewer::ShowError()
{
    if (_item != nullptr)
    {
        errorTextBlock->Text = _item->Error;
    }
	errorTextBlock->Opacity = 1;
}

// <summary>
// Visualize the Image associated with the data item by updating the Image 
// object and setting Opacity to 1.
// </summary>
void ItemViewer::ShowImage()
{
    if (_item != nullptr)
    {
        image->Source = _item->Image;

		// If the orientation is horizontal
		if (!(_item->OrientationCoalesced >= 2U && _item->OrientationCoalesced <= 8U))
		{
			// Clear any current render transforms
			image->ClearValue(Image::RenderTransformProperty);
		}
		// else perform the matrix calculations
		else
		{
			Windows::UI::Xaml::Media::MatrixTransform^ _MatrixTransform = ref new Windows::UI::Xaml::Media::MatrixTransform();

			_MatrixTransform->Matrix = OrientationHelper::GetInverseMatrix(_item->OrientationCoalesced);

			image->RenderTransform = _MatrixTransform;
		}
    }
    image->Opacity = 1;
}

// <summary>
// Drop all references to the data item
// </summary>
void ItemViewer::ClearData()
{
    _item = nullptr;
    titleTextBlock->ClearValue(TextBlock::TextProperty);
	errorTextBlock->ClearValue(TextBlock::TextProperty);
    image->ClearValue(Image::SourceProperty);
}