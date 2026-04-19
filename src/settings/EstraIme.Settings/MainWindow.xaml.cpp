#include "MainWindow.xaml.h"

#include "../../common/EstraIme.Common/Log.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>
#include <winrt/Windows.UI.Text.h>
#include <winrt/base.h>

using namespace winrt;
using namespace Microsoft::UI;
using namespace Microsoft::UI::Dispatching;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Controls;
using namespace Microsoft::UI::Xaml::Media;

namespace
{
    TextBlock CreateLabel(hstring const& text)
    {
        TextBlock label;
        label.Text(text);
        label.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        return label;
    }
}

namespace winrt::EstraIme::Settings::implementation
{
    MainWindow::MainWindow()
    {
        try
        {
            Title(L"EstraIME Settings");
            BuildUi();
            LoadConfig();
            ApplyConfigToControls();
            Activated([this](IInspectable const&, WindowActivatedEventArgs const&) {
                RefreshHealth();
            });
        }
        catch (winrt::hresult_error const& error)
        {
            LogFailure(L"MainWindow construction", error);
            throw;
        }
    }

    void MainWindow::BuildUi()
    {
        StackPanel root;
        root.Spacing(12);
        root.Padding(ThicknessHelper::FromLengths(20, 20, 20, 20));
        root.Background(SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 250, 250, 250)));

        TextBlock title;
        title.Text(L"EstraIME Alpha Settings");
        title.FontSize(24);
        title.FontWeight(Windows::UI::Text::FontWeights::SemiBold());
        title.Foreground(SolidColorBrush(Windows::UI::ColorHelper::FromArgb(255, 20, 20, 20)));
        root.Children().Append(title);

        llmEnabledSwitch_ = ToggleSwitch();
        llmEnabledSwitch_.Header(box_value(L"Enable asynchronous LLM autocomplete"));
        root.Children().Append(llmEnabledSwitch_);

        root.Children().Append(CreateLabel(L"Provider"));
        providerCombo_ = ComboBox();
        providerCombo_.Items().Append(box_value(L"mock"));
        providerCombo_.Items().Append(box_value(L"local"));
        providerCombo_.Items().Append(box_value(L"cloud"));
        root.Children().Append(providerCombo_);

        root.Children().Append(CreateLabel(L"Local model"));
        modelCombo_ = ComboBox();
        modelCombo_.Items().Append(box_value(L"qwen3-1.7b-q4"));
        modelCombo_.Items().Append(box_value(L"qwen3-1.7b-q5"));
        modelCombo_.Items().Append(box_value(L"qwen2.5-3b-q4"));
        root.Children().Append(modelCombo_);

        root.Children().Append(CreateLabel(L"Local endpoint"));
        endpointTextBox_ = TextBox();
        endpointTextBox_.PlaceholderText(L"http://127.0.0.1:8080/v1/chat/completions");
        root.Children().Append(endpointTextBox_);

        root.Children().Append(CreateLabel(L"Pause before triggering autocomplete (ms)"));
        pauseSlider_ = Slider();
        pauseSlider_.Minimum(50);
        pauseSlider_.Maximum(1000);
        pauseSlider_.StepFrequency(10);
        root.Children().Append(pauseSlider_);

        root.Children().Append(CreateLabel(L"Minimum characters before trigger"));
        minCharsBox_ = NumberBox();
        minCharsBox_.Minimum(1);
        minCharsBox_.Maximum(32);
        minCharsBox_.SmallChange(1);
        root.Children().Append(minCharsBox_);

        cloudOptInCheckBox_ = CheckBox();
        cloudOptInCheckBox_.Content(box_value(L"Allow optional cloud enhancement"));
        root.Children().Append(cloudOptInCheckBox_);

        StackPanel buttonRow;
        buttonRow.Orientation(Orientation::Horizontal);
        buttonRow.Spacing(8);

        Button saveButton;
        saveButton.Content(box_value(L"Save"));
        saveButton.Click([this](IInspectable const&, RoutedEventArgs const&) {
            SaveConfig();
        });
        buttonRow.Children().Append(saveButton);

        Button refreshButton;
        refreshButton.Content(box_value(L"Refresh sidecar health"));
        refreshButton.Click([this](IInspectable const&, RoutedEventArgs const&) {
            RefreshHealth();
        });
        buttonRow.Children().Append(refreshButton);

        root.Children().Append(buttonRow);

        statusText_ = TextBlock();
        statusText_.Text(L"sidecar: unknown");
        root.Children().Append(statusText_);

        configPathText_ = TextBlock();
        configPathText_.Text(hstring(std::wstring(L"Config path: ") + ::EstraIme::Common::ConfigStore::DefaultPath()));
        configPathText_.TextWrapping(TextWrapping::Wrap);
        root.Children().Append(configPathText_);

        ScrollViewer scroller;
        scroller.Content(root);
        Content(scroller);
    }

    void MainWindow::LoadConfig()
    {
        config_ = ::EstraIme::Common::ConfigStore::Load();
    }

    void MainWindow::ApplyConfigToControls()
    {
        llmEnabledSwitch_.IsOn(config_.llmEnabled);
        providerCombo_.SelectedItem(box_value(hstring(config_.llmProvider)));
        modelCombo_.SelectedItem(box_value(hstring(config_.localLlm.modelId)));
        endpointTextBox_.Text(hstring(config_.localLlm.endpoint));
        pauseSlider_.Value(config_.trigger.pauseMs);
        minCharsBox_.Value(config_.trigger.minChars);
        cloudOptInCheckBox_.IsChecked(config_.cloudOptIn);
    }

    void MainWindow::SaveConfig()
    {
        config_.llmEnabled = llmEnabledSwitch_.IsOn();
        if (auto provider = providerCombo_.SelectedItem())
        {
            config_.llmProvider = unbox_value<hstring>(provider).c_str();
        }
        if (auto model = modelCombo_.SelectedItem())
        {
            config_.localLlm.modelId = unbox_value<hstring>(model).c_str();
        }
        config_.localLlm.endpoint = endpointTextBox_.Text().c_str();
        config_.trigger.pauseMs = static_cast<int>(pauseSlider_.Value());
        config_.trigger.minChars = static_cast<int>(minCharsBox_.Value());
        config_.cloudOptIn = cloudOptInCheckBox_.IsChecked().Value();

        ::EstraIme::Common::ConfigStore::Save(config_);
        statusText_.Text(L"Configuration saved");
        RefreshHealth();
    }

    void MainWindow::RefreshHealth()
    {
        RefreshHealthAsync();
    }

    fire_and_forget MainWindow::RefreshHealthAsync()
    {
        auto lifetime = get_strong();
        auto dispatcher = DispatcherQueue();
        co_await winrt::resume_background();
        const auto summary = sidecar_.GetHealthSummary();
        dispatcher.TryEnqueue([this, summary]() {
            statusText_.Text(hstring(summary));
        });
    }

    void MainWindow::LogFailure(std::wstring const& step, winrt::hresult_error const& error)
    {
        ::EstraIme::Common::Logger::Error(L"Settings " + step + L" failed: " + std::wstring(error.message()));
    }
}
