#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

namespace winrt::EstraIme::Settings::implementation
{
    struct MainWindow : winrt::Microsoft::UI::Xaml::WindowT<MainWindow>
    {
        MainWindow();
    };
}
