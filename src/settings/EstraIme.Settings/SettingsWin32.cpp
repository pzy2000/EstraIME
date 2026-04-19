#include "../../common/EstraIme.Common/ConfigStore.h"
#include "../../common/EstraIme.Common/IpcClient.h"

#include <Windows.h>
#include <CommCtrl.h>
#include <windowsx.h>

#include <algorithm>
#include <string>

namespace
{
    constexpr int kIdLlmEnabled = 1001;
    constexpr int kIdProvider = 1002;
    constexpr int kIdModel = 1003;
    constexpr int kIdEndpoint = 1004;
    constexpr int kIdPauseMs = 1005;
    constexpr int kIdMinChars = 1006;
    constexpr int kIdCloudOptIn = 1007;
    constexpr int kIdSave = 1008;
    constexpr int kIdRefresh = 1009;
    constexpr int kIdStatus = 1010;

    struct SettingsState
    {
        EstraIme::ImeConfig config{};
        EstraIme::Common::SidecarClient sidecar{};
    };

    HWND Control(HWND hwnd, int id)
    {
        return GetDlgItem(hwnd, id);
    }

    HMENU ControlId(int id)
    {
        return reinterpret_cast<HMENU>(static_cast<INT_PTR>(id));
    }

    std::wstring GetText(HWND hwnd, int id)
    {
        const HWND control = Control(hwnd, id);
        const int length = GetWindowTextLengthW(control);
        std::wstring value(length, L'\0');
        GetWindowTextW(control, value.data(), length + 1);
        return value;
    }

    void SetText(HWND hwnd, int id, const std::wstring& value)
    {
        SetWindowTextW(Control(hwnd, id), value.c_str());
    }

    int GetInt(HWND hwnd, int id, int fallback)
    {
        try
        {
            return std::stoi(GetText(hwnd, id));
        }
        catch (...)
        {
            return fallback;
        }
    }

    std::wstring ComboText(HWND hwnd, int id)
    {
        const HWND combo = Control(hwnd, id);
        const int selected = static_cast<int>(SendMessageW(combo, CB_GETCURSEL, 0, 0));
        if (selected < 0)
        {
            return {};
        }

        wchar_t buffer[256]{};
        SendMessageW(combo, CB_GETLBTEXT, selected, reinterpret_cast<LPARAM>(buffer));
        return buffer;
    }

    void SelectComboText(HWND hwnd, int id, const std::wstring& value)
    {
        const HWND combo = Control(hwnd, id);
        const auto index = SendMessageW(combo, CB_FINDSTRINGEXACT, static_cast<WPARAM>(-1), reinterpret_cast<LPARAM>(value.c_str()));
        if (index != CB_ERR)
        {
            SendMessageW(combo, CB_SETCURSEL, index, 0);
        }
    }

    void AddComboItem(HWND hwnd, int id, const wchar_t* text)
    {
        SendMessageW(Control(hwnd, id), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
    }

    void CreateLabel(HWND parent, int x, int y, int w, const wchar_t* text)
    {
        CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, 22, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    }

    void LoadControls(HWND hwnd, SettingsState& state)
    {
        state.config = EstraIme::Common::ConfigStore::Load();

        Button_SetCheck(Control(hwnd, kIdLlmEnabled), state.config.llmEnabled ? BST_CHECKED : BST_UNCHECKED);
        SelectComboText(hwnd, kIdProvider, state.config.llmProvider);
        SelectComboText(hwnd, kIdModel, state.config.localLlm.modelId);
        SetText(hwnd, kIdEndpoint, state.config.localLlm.endpoint);
        SetText(hwnd, kIdPauseMs, std::to_wstring(state.config.trigger.pauseMs));
        SetText(hwnd, kIdMinChars, std::to_wstring(state.config.trigger.minChars));
        Button_SetCheck(Control(hwnd, kIdCloudOptIn), state.config.cloudOptIn ? BST_CHECKED : BST_UNCHECKED);
    }

    void SaveControls(HWND hwnd, SettingsState& state)
    {
        state.config.llmEnabled = Button_GetCheck(Control(hwnd, kIdLlmEnabled)) == BST_CHECKED;
        state.config.llmProvider = ComboText(hwnd, kIdProvider);
        state.config.localLlm.modelId = ComboText(hwnd, kIdModel);
        state.config.localLlm.endpoint = GetText(hwnd, kIdEndpoint);
        state.config.trigger.pauseMs = std::clamp(GetInt(hwnd, kIdPauseMs, 180), 50, 2000);
        state.config.trigger.minChars = std::clamp(GetInt(hwnd, kIdMinChars, 4), 1, 64);
        state.config.cloudOptIn = Button_GetCheck(Control(hwnd, kIdCloudOptIn)) == BST_CHECKED;

        EstraIme::Common::ConfigStore::Save(state.config);
        SetText(hwnd, kIdStatus, L"Saved. IME and sidecar will use the updated config on the next request.");
    }

    void RefreshHealth(HWND hwnd, SettingsState& state)
    {
        SetText(hwnd, kIdStatus, state.sidecar.GetHealthSummary());
    }

    LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        auto* state = reinterpret_cast<SettingsState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        switch (message)
        {
        case WM_CREATE:
        {
            auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
            state = reinterpret_cast<SettingsState*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

            CreateLabel(hwnd, 24, 24, 360, L"EstraIME Alpha Settings");
            CreateWindowExW(0, L"BUTTON", L"Enable asynchronous LLM autocomplete", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 64, 340, 24, hwnd, ControlId(kIdLlmEnabled), GetModuleHandleW(nullptr), nullptr);

            CreateLabel(hwnd, 24, 104, 160, L"Provider");
            CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 190, 100, 260, 120, hwnd, ControlId(kIdProvider), GetModuleHandleW(nullptr), nullptr);
            AddComboItem(hwnd, kIdProvider, L"mock");
            AddComboItem(hwnd, kIdProvider, L"local");
            AddComboItem(hwnd, kIdProvider, L"cloud");

            CreateLabel(hwnd, 24, 144, 160, L"Local model");
            CreateWindowExW(0, WC_COMBOBOXW, nullptr, WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 190, 140, 260, 120, hwnd, ControlId(kIdModel), GetModuleHandleW(nullptr), nullptr);
            AddComboItem(hwnd, kIdModel, L"qwen3-1.7b-q4");
            AddComboItem(hwnd, kIdModel, L"qwen3-1.7b-q5");
            AddComboItem(hwnd, kIdModel, L"qwen2.5-3b-q4");

            CreateLabel(hwnd, 24, 184, 160, L"Local endpoint");
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 190, 180, 520, 24, hwnd, ControlId(kIdEndpoint), GetModuleHandleW(nullptr), nullptr);

            CreateLabel(hwnd, 24, 224, 160, L"Pause ms");
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_NUMBER, 190, 220, 120, 24, hwnd, ControlId(kIdPauseMs), GetModuleHandleW(nullptr), nullptr);

            CreateLabel(hwnd, 24, 264, 160, L"Minimum chars");
            CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", nullptr, WS_CHILD | WS_VISIBLE | ES_NUMBER, 190, 260, 120, 24, hwnd, ControlId(kIdMinChars), GetModuleHandleW(nullptr), nullptr);

            CreateWindowExW(0, L"BUTTON", L"Allow optional cloud enhancement", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 24, 304, 320, 24, hwnd, ControlId(kIdCloudOptIn), GetModuleHandleW(nullptr), nullptr);

            CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 24, 352, 100, 30, hwnd, ControlId(kIdSave), GetModuleHandleW(nullptr), nullptr);
            CreateWindowExW(0, L"BUTTON", L"Refresh health", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 140, 352, 140, 30, hwnd, ControlId(kIdRefresh), GetModuleHandleW(nullptr), nullptr);
            CreateWindowExW(0, L"STATIC", L"sidecar: unknown", WS_CHILD | WS_VISIBLE, 24, 404, 720, 48, hwnd, ControlId(kIdStatus), GetModuleHandleW(nullptr), nullptr);

            LoadControls(hwnd, *state);
            RefreshHealth(hwnd, *state);
            return 0;
        }
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case kIdSave:
                SaveControls(hwnd, *state);
                return 0;
            case kIdRefresh:
                RefreshHealth(hwnd, *state);
                return 0;
            default:
                break;
            }
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wParam, lParam);
    }
}

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&controls);

    const wchar_t className[] = L"EstraImeSettingsWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    SettingsState state{};
    HWND window = CreateWindowExW(
        0,
        className,
        L"EstraIME Settings",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        800,
        520,
        nullptr,
        nullptr,
        instance,
        &state);

    if (!window)
    {
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return 0;
}
