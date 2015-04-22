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
// showing a placehlder, we set the opacity of other elements to zero
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

		//Windows::UI::Xaml::Media::TransformGroup^ _TransformGroup = ref new Windows::UI::Xaml::Media::TransformGroup();

		//Windows::UI::Xaml::Media::RotateTransform^ _RotateTransform = ref new Windows::UI::Xaml::Media::RotateTransform();

		//Windows::UI::Xaml::Media::ScaleTransform^ _ScaleTransform = ref new Windows::UI::Xaml::Media::ScaleTransform();

		Windows::UI::Xaml::Media::MatrixTransform^ _MatrixTransform = ref new Windows::UI::Xaml::Media::MatrixTransform();

		switch (_item->OrientationFlag)
		{
		case 2U:
			//_ScaleTransform->ScaleX = -1.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				-1.0, 0.0, //0.0
				0.0, 1.0,  //0.0
				0.0, 0.0   //1.0
				);
			break;
		case 3U:
			//_RotateTransform->Angle = 180.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				-1.0, 0.0, //0.0
				0.0, -1.0, //0.0
				0.0, 0.0   //1.0
				);
			break;
		case 4U:
			//_ScaleTransform->ScaleY = -1.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				1.0, 0.0,  //0.0
				0.0, -1.0, //0.0
				0.0, 0.0   //1.0
				);
			break;
		case 5U:
			//_RotateTransform->Angle = 90.0;
			//_ScaleTransform->ScaleX = -1.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				0.0, -1.0, //0.0
				-1.0, 0.0, //0.0
				0.0, 0.0   //1.0
				);
			break;
		case 6U:
			//_RotateTransform->Angle = 270.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				0.0, -1.0, //0.0
				1.0, 0.0,  //0.0
				0.0, 0.0   //1.0
				);
			break;
		case 7U:
			//_RotateTransform->Angle = 270.0;
			//_ScaleTransform->ScaleX = -1.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				0.0, -1.0, //0.0
				-1.0, 0.0, //0.0
				0.0, 0.0   //1.0
				);
			break;
		case 8U:
			//_RotateTransform->Angle = 90.0;
			_MatrixTransform->Matrix = Windows::UI::Xaml::Media::Matrix(
				0.0, 1.0,  //0.0
				-1.0, 0.0, //0.0
				0.0, 0.0   //1.0
				);
			break;
		}

		//_TransformGroup->Children->Append(_RotateTransform);
		//_TransformGroup->Children->Append(_ScaleTransform);

		//image->RenderTransform = _TransformGroup;

		image->RenderTransform = _MatrixTransform;
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