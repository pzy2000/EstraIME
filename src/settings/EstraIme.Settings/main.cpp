#include "App.xaml.h"

#include "../../common/EstraIme.Common/Log.h"
#include "../../common/EstraIme.Common/TextUtils.h"

#include <Windows.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/base.h>

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        winrt::Microsoft::UI::Xaml::Application::Start([](auto&&) {
            winrt::make<winrt::EstraIme::Settings::implementation::App>();
        });
        return 0;
    }
    catch (winrt::hresult_error const& error)
    {
        ::EstraIme::Common::Logger::Error(L"Settings fatal hresult_error: " + std::wstring(error.message()));
    }
    catch (std::exception const& error)
    {
        ::EstraIme::Common::Logger::Error(::EstraIme::Common::Utf8ToWide(error.what()));
    }
    return 1;
}
