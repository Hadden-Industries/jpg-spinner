//
// Scenario_AfterPick.xaml.h
// Declaration of the Scenario_AfterPick class
//

#pragma once

#include "pch.h"
#include "Scenario_AfterPick.g.h"
#include "MainPage.xaml.h"

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
		~Scenario_AfterPick();
		Data^ storeData;
        TypedEventHandler<ListViewBase^, ContainerContentChangingEventArgs^>^ _delegate;
		IWICImagingFactory * pIWICImagingFactory;
		bool CoInitializeExSucceeded;
		uint32_t numberProcessorsToUse;
		uint64_t bytesRAMToUse;
		volatile std::atomic_ulong imagesSelected;
		volatile std::atomic_ulong imagesAnalysed;
		volatile std::atomic_ulong imagesToBeRotated;
		volatile std::atomic_ulong imagesRotated;
		volatile std::atomic_ulong imagesErrored;
		volatile std::atomic_ulong imagesBeingRotated;
		Windows::UI::Core::CoreDispatcher^ _dispatcher;
		Windows::ApplicationModel::Resources::ResourceLoader^ _resourceLoader;
		bool _CropChecked;
		bool _ProgressiveChecked;

		Windows::System::Threading::ThreadPoolTimer^ _updateStatusTextTimer;
		volatile std::atomic_bool _stoppingUpdateStatusText;
		void UpdateStatusText();

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
	};
}
