//
// OrientationHelper.h
// Declaration of the OrientationHelper class
//

#pragma once

#include "pch.h"

class OrientationHelper
{
public:
	OrientationHelper::OrientationHelper(const unsigned char orientation);

	Windows::UI::Xaml::Media::Matrix GetMatrix();
	static Windows::UI::Xaml::Media::Matrix GetMatrix(const unsigned char orientation);

	Windows::UI::Xaml::Media::Matrix GetInverseMatrix();
	static Windows::UI::Xaml::Media::Matrix GetInverseMatrix(const unsigned char orientation);

	bool XYFlips();
	static bool XYFlips(const unsigned char orientation);

	WICBitmapTransformOptions GetWICBitmapTransformOptions();
	static WICBitmapTransformOptions GetWICBitmapTransformOptions(const unsigned char orientation);

private:
	unsigned char _orientation;
};