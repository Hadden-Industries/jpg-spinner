/*
* Comment from jpegexiforient.c
* 
* Here is what the letter F would look like if it were tagged correctly
* and displayed by a program that ignores the orientation tag:
*
*   1        2       3      4         5            6           7          8
*
* 888888  888888      88  88      8888888888  88                  88  8888888888
* 88          88      88  88      88  88      88  88          88  88      88  88
* 8888      8888    8888  8888    88          8888888888  8888888888          88
* 88          88      88  88
* 88          88  888888  888888
*
*/

#include "pch.h"
#include "OrientationHelper.h"

OrientationHelper::OrientationHelper(const unsigned char orientation)
{
	// Check for invalid argument
	if (orientation < 1U || orientation > 8U)
	{
		throw Platform::Exception::CreateException(E_INVALIDARG);
	}

	_orientation = orientation;
}

Windows::UI::Xaml::Media::Matrix OrientationHelper::getMatrix()
{
	return OrientationHelper::getMatrix(_orientation);
}

Windows::UI::Xaml::Media::Matrix OrientationHelper::getMatrix(const unsigned char orientation)
{
	switch (orientation)
	{
	case 2U:
		return Windows::UI::Xaml::Media::Matrix(
			-1.0, 0.0, //0.0
			0.0, 1.0,  //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 3U:
		return Windows::UI::Xaml::Media::Matrix(
			-1.0, 0.0, //0.0
			0.0, -1.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 4U:
		return Windows::UI::Xaml::Media::Matrix(
			1.0, 0.0,  //0.0
			0.0, -1.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 5U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, 1.0, //0.0
			1.0, 0.0, //0.0
			0.0, 0.0  //1.0
			);
		break;

	case 6U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, 1.0,  //0.0
			-1.0, 0.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 7U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			-1.0, 0.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 8U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			1.0, 0.0,  //0.0
			0.0, 0.0   //1.0
			);
		break;

	default:
		return Windows::UI::Xaml::Media::MatrixHelper::Identity;
	}
}

Windows::UI::Xaml::Media::Matrix OrientationHelper::getInverseMatrix()
{
	return OrientationHelper::getInverseMatrix(_orientation);
}

Windows::UI::Xaml::Media::Matrix OrientationHelper::getInverseMatrix(const unsigned char orientation)
{
	switch (orientation)
	{
	case 2U:
		return Windows::UI::Xaml::Media::Matrix(
			-1.0, 0.0, //0.0
			0.0, 1.0,  //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 3U:
		return Windows::UI::Xaml::Media::Matrix(
			-1.0, 0.0, //0.0
			0.0, -1.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 4U:
		return Windows::UI::Xaml::Media::Matrix(
			1.0, 0.0,  //0.0
			0.0, -1.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 5U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, 1.0, //0.0
			1.0, 0.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 6U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			1.0, 0.0,  //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 7U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			-1.0, 0.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	case 8U:
		return Windows::UI::Xaml::Media::Matrix(
			0.0, 1.0,  //0.0
			-1.0, 0.0, //0.0
			0.0, 0.0   //1.0
			);
		break;

	default:
		return Windows::UI::Xaml::Media::MatrixHelper::Identity;
	}
}

bool OrientationHelper::XYFlips()
{
	return OrientationHelper::XYFlips(_orientation);
}

bool OrientationHelper::XYFlips(const unsigned char orientation)
{
	switch (orientation)
	{
	case 5U:
	case 6U:
	case 7U:
	case 8U:
		return true;
		break;

	default:
		return false;
	}
}

WICBitmapTransformOptions OrientationHelper::GetWICBitmapTransformOptions()
{
	return OrientationHelper::GetWICBitmapTransformOptions(_orientation);
}

WICBitmapTransformOptions OrientationHelper::GetWICBitmapTransformOptions(const unsigned char orientation)
{
	switch (orientation)
	{
	case 2U:
		return WICBitmapTransformFlipHorizontal;
		break;

	case 3U:
		return WICBitmapTransformRotate180;
		break;

	case 4U:
		return WICBitmapTransformFlipVertical;
		break;

	case 5U:
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ee690131%28v=vs.85%29.aspx "Rotations are done before the flip." - seems to be the reverse
		return static_cast<WICBitmapTransformOptions>(WICBitmapTransformFlipHorizontal | WICBitmapTransformRotate270);
		break;

	case 6U:
		return WICBitmapTransformRotate90;
		break;

	case 7U:
		// https://msdn.microsoft.com/en-us/library/windows/desktop/ee690131%28v=vs.85%29.aspx "Rotations are done before the flip." - seems to be the reverse
		return static_cast<WICBitmapTransformOptions>(WICBitmapTransformFlipVertical | WICBitmapTransformRotate270);
		break;

	case 8U:
		return WICBitmapTransformRotate270;
		break;

	default:
		return WICBitmapTransformRotate0;
	}
}