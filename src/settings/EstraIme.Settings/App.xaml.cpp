#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace winrt::EstraIme::Settings::implementation
{
    App::App()
    {
        InitializeComponent();
    }

    void App::OnLaunched(LaunchActivatedEventArgs const&)
    {
        window_ = make<MainWindow>();
        window_.Activate();
    }
}
