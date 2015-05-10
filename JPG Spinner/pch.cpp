//
// pch.cpp
// Include the standard header and generate the precompiled header.
//

#include "pch.h"

Platform::String^ HResultToHexString(HRESULT hr)
{
	std::wostringstream os;

	os << L"0x";

	os << std::hex << std::uppercase << hr;

	return ref new Platform::String(os.str().data());
}

OrientationHelper::OrientationHelper(unsigned char orientation, unsigned short xOffset, unsigned short yOffset)
{
	switch (orientation)
	{
	case 1U:
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			1.0, 0.0, //0.0
			0.0, 1.0,  //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = false;
		_WICBitmapTransformOptions = WICBitmapTransformRotate0;
		break;

	case 2U:
		//_ScaleTransform->ScaleX = -1.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			-1.0, 0.0, //0.0
			0.0, 1.0,  //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = false;
		_WICBitmapTransformOptions = WICBitmapTransformFlipHorizontal;
		break;

	case 3U:
		//_RotateTransform->Angle = 180.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			-1.0, 0.0, //0.0
			0.0, -1.0, //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = false;
		_WICBitmapTransformOptions = WICBitmapTransformRotate180;
		break;

	case 4U:
		//_ScaleTransform->ScaleY = -1.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			1.0, 0.0,  //0.0
			0.0, -1.0, //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = false;
		_WICBitmapTransformOptions = WICBitmapTransformFlipVertical;
		break;

	case 5U:
		//_RotateTransform->Angle = 90.0;
		//_ScaleTransform->ScaleX = -1.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			-1.0, 0.0, //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = true;
		_WICBitmapTransformOptions = WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal;
		break;

	case 6U:
		//_RotateTransform->Angle = 270.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			1.0, 0.0,  //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = true;
		_WICBitmapTransformOptions = WICBitmapTransformRotate90;
		break;

	case 7U:
		//_RotateTransform->Angle = 270.0;
		//_ScaleTransform->ScaleX = -1.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			0.0, -1.0, //0.0
			-1.0, 0.0, //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_XYFlips = true;
		_WICBitmapTransformOptions = WICBitmapTransformRotate90 | WICBitmapTransformFlipVertical;
		break;

	case 8U:
		//_RotateTransform->Angle = 90.0;
		_Matrix = Windows::UI::Xaml::Media::Matrix(
			0.0, 1.0,  //0.0
			-1.0, 0.0, //0.0
			static_cast<double>(xOffset), static_cast<double>(yOffset) //1.0
			);
		_WICBitmapTransformOptions = WICBitmapTransformRotate270;
		_XYFlips = true;
		break;

	default:
		throw Platform::Exception::CreateException(E_INVALIDARG);
	}
}

Windows::UI::Xaml::Media::Matrix OrientationHelper::getInverseMatrix()
{
	return _Matrix;
}

Windows::UI::Xaml::Media::Matrix OrientationHelper::getMatrix()
{
	auto d2D1Matrix = D2D1::Matrix3x2F(static_cast<float>(_Matrix.M11), static_cast<float>(_Matrix.M12), static_cast<float>(_Matrix.M21), static_cast<float>(_Matrix.M22), static_cast<float>(_Matrix.OffsetX), static_cast<float>(_Matrix.OffsetY));

	if (d2D1Matrix.Invert())
	{
		return Windows::UI::Xaml::Media::Matrix(d2D1Matrix._11, d2D1Matrix._12, d2D1Matrix._21, d2D1Matrix._22, d2D1Matrix._31, d2D1Matrix._32);
	}
	else
	{
		return Windows::UI::Xaml::Media::Matrix::Identity;
	}
}

WICBitmapTransformOptions OrientationHelper::GetWICBitmapTransformOptions()
{
	return static_cast<WICBitmapTransformOptions>(_WICBitmapTransformOptions);
}

bool OrientationHelper::XYFlips()
{
	return _XYFlips;
}