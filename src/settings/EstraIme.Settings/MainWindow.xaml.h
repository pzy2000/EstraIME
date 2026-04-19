#pragma once

#include "../../common/EstraIme.Common/ConfigStore.h"
#include "../../common/EstraIme.Common/IpcClient.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace winrt::EstraIme::Settings::implementation
{
    struct MainWindow : winrt::Microsoft::UI::Xaml::WindowT<MainWindow>
    {
        MainWindow();

    private:
        void BuildUi();
        void LoadConfig();
        void ApplyConfigToControls();
        void SaveConfig();
        void RefreshHealth();
        winrt::fire_and_forget RefreshHealthAsync();
        void LogFailure(std::wstring const& step, winrt::hresult_error const& error);

        ::EstraIme::ImeConfig config_{};
        ::EstraIme::Common::SidecarClient sidecar_{};

        winrt::Microsoft::UI::Xaml::Controls::ToggleSwitch llmEnabledSwitch_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox providerCombo_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::ComboBox modelCombo_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::Slider pauseSlider_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::NumberBox minCharsBox_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::CheckBox cloudOptInCheckBox_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock statusText_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBox endpointTextBox_{nullptr};
        winrt::Microsoft::UI::Xaml::Controls::TextBlock configPathText_{nullptr};
    };
}
