#include "App.xaml.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/base.h>

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
        winrt::make<winrt::EstraIme::Settings::implementation::App>();
    });
    return 0;
}
