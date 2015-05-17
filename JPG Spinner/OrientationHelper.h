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

	Windows::UI::Xaml::Media::Matrix getMatrix();
	static Windows::UI::Xaml::Media::Matrix getMatrix(const unsigned char orientation);

	Windows::UI::Xaml::Media::Matrix getInverseMatrix();
	static Windows::UI::Xaml::Media::Matrix getInverseMatrix(const unsigned char orientation);

	bool XYFlips();
	static bool XYFlips(const unsigned char orientation);

	WICBitmapTransformOptions GetWICBitmapTransformOptions();
	static WICBitmapTransformOptions GetWICBitmapTransformOptions(const unsigned char orientation);

private:
	unsigned char _orientation;
};