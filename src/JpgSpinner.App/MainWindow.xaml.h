#pragma once

#include "MainWindow.g.h"

namespace winrt::JpgSpinner::implementation
{
// winrt::make uses generated heap implementation machinery that derives from
// this runtime-class implementation, so the implementation cannot be final.
struct MainWindow : MainWindowT<MainWindow>
{
    MainWindow() = default;
};
}

namespace winrt::JpgSpinner::factory_implementation
{
struct MainWindow : MainWindowT<MainWindow, implementation::MainWindow>
{
};
}
