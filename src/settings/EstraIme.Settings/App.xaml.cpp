#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include "../../common/EstraIme.Common/Log.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::EstraIme::Settings::implementation
{
    App::App()
    {
        RequestedTheme(ApplicationTheme::Light);
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        try
        {
            window_ = make<MainWindow>();
            window_.Activate();
        }
        catch (winrt::hresult_error const& error)
        {
            ::EstraIme::Common::Logger::Error(L"Settings App launch failed: " + std::wstring(error.message()));
            throw;
        }
    }
}
