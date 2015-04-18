//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// Scenario1.xaml.h
// Declaration of the Scenario1 class
//

#pragma once

#include "pch.h"
#include "Scenario_AfterPick.g.h"
#include "MainPage.xaml.h"
#include <sstream> // wostringstream

using namespace Windows::Foundation;
using namespace Windows::UI::Xaml::Controls;

namespace JPG_Spinner
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    [Windows::Foundation::Metadata::WebHostHiddenAttribute]
	public ref class Scenario_AfterPick sealed
    {
    public:
		Scenario_AfterPick();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
		Scenario_AfterPick::~Scenario_AfterPick();
        MainPage^ rootPage;
		Data^ storeData;
        TypedEventHandler<ListViewBase^, ContainerContentChangingEventArgs^>^ _delegate;
		IWICImagingFactory * pIWICImagingFactory;
		BOOL CoInitializeExSucceeded;
		std::atomic_ulong imagesAnalysed;
		std::atomic_ulong imagesToBeRotated;
		std::atomic_ulong imagesRotated;
		Windows::UI::Core::CoreDispatcher^ _dispatcher;
		Windows::ApplicationModel::Resources::ResourceLoader^ _resourceLoader;

        void ItemGridView_ContainerContentChanging(ListViewBase^ sender, Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs^ e);

        // Managing delegate creation to ensure we instantiate a single instance for optimal performance
        property TypedEventHandler<ListViewBase^, ContainerContentChangingEventArgs^>^ ContainerContentChangingDelegate
        {
            TypedEventHandler<ListViewBase^, ContainerContentChangingEventArgs^>^ get()
            {
                if (_delegate == nullptr)
                {
                    _delegate = ref new TypedEventHandler<ListViewBase^, ContainerContentChangingEventArgs^>(
                        this,
						&JPG_Spinner::Scenario_AfterPick::ItemGridView_ContainerContentChanging);
                }
                return _delegate;
            }
        }
		void GridView1_Loaded(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};
}
