#include "TipGuids.h"

#include "../../common/EstraIme.Common/Log.h"

#include <Windows.h>
#include <msctf.h>

#include <format>
#include <string>

namespace
{
    using InstallLayoutOrTipFn = BOOL(WINAPI*)(LPCWSTR, DWORD);

    constexpr DWORD kIlotUninstall = 0x00000001;
    constexpr LANGID kZhCnLangId = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);

    std::wstring GuidToString(REFGUID guid)
    {
        wchar_t buffer[64]{};
        StringFromGUID2(guid, buffer, static_cast<int>(std::size(buffer)));
        return buffer;
    }

    HRESULT SetRegistryString(HKEY root, const std::wstring& subKey, const std::wstring& name, const std::wstring& value)
    {
        HKEY key = nullptr;
        const auto createStatus = RegCreateKeyExW(root, subKey.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, nullptr);
        if (createStatus != ERROR_SUCCESS)
        {
            return HRESULT_FROM_WIN32(createStatus);
        }

        const auto status = RegSetValueExW(
            key,
            name.empty() ? nullptr : name.c_str(),
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()),
            static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
        return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
    }

    HRESULT RegisterCategories()
    {
        ITfCategoryMgr* categories = nullptr;
        auto hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, reinterpret_cast<void**>(&categories));
        if (FAILED(hr))
        {
            return hr;
        }

        const auto cleanup = [&categories]() {
            if (categories)
            {
                categories->Release();
                categories = nullptr;
            }
        };

        hr = categories->RegisterCategory(CLSID_EstraImeTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_EstraImeTextService);
        if (FAILED(hr))
        {
            cleanup();
            return hr;
        }

        hr = categories->RegisterCategory(CLSID_EstraImeTextService, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, CLSID_EstraImeTextService);
        cleanup();
        return hr;
    }

    HRESULT RegisterProfile(const wchar_t* modulePath)
    {
        ITfInputProcessorProfileMgr* profileMgr = nullptr;
        auto hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profileMgr));
        if (FAILED(hr))
        {
            return hr;
        }

        hr = profileMgr->RegisterProfile(
            CLSID_EstraImeTextService,
            kZhCnLangId,
            GUID_EstraImeProfile,
            L"EstraIME Alpha",
            static_cast<ULONG>(wcslen(L"EstraIME Alpha")),
            modulePath,
            static_cast<ULONG>(wcslen(modulePath)),
            0,
            nullptr,
            0,
            TRUE,
            0);

        profileMgr->Release();
        return hr;
    }

    HRESULT InstallOrRemoveForCurrentUser(const bool uninstall)
    {
        const auto clsid = GuidToString(CLSID_EstraImeTextService);
        const auto profile = GuidToString(GUID_EstraImeProfile);
        const auto spec = std::format(L"0x{:04X}:{}{}", static_cast<unsigned>(kZhCnLangId), clsid, profile);

        const auto library = LoadLibraryW(L"Msctf.dll");
        if (!library)
        {
            return HRESULT_FROM_WIN32(GetLastError());
        }

        const auto cleanup = [&library]() { FreeLibrary(library); };
        const auto installFn = reinterpret_cast<InstallLayoutOrTipFn>(GetProcAddress(library, "InstallLayoutOrTip"));
        if (!installFn)
        {
            cleanup();
            return HRESULT_FROM_WIN32(GetLastError());
        }

        const auto ok = installFn(spec.c_str(), uninstall ? kIlotUninstall : 0);
        cleanup();
        return ok ? S_OK : E_FAIL;
    }
}

HRESULT RegisterServer(HMODULE moduleHandle)
{
    auto logFailure = [](const wchar_t* step, const HRESULT hr) {
        EstraIme::Common::Logger::Error(std::format(L"RegisterServer failed at {}: 0x{:08X}", step, static_cast<unsigned>(hr)));
    };

    wchar_t modulePath[MAX_PATH]{};
    GetModuleFileNameW(moduleHandle, modulePath, MAX_PATH);

    const auto clsid = GuidToString(CLSID_EstraImeTextService);
    const auto clsidPath = L"CLSID\\" + clsid;
    auto hr = SetRegistryString(HKEY_CLASSES_ROOT, clsidPath, L"", L"EstraIME Alpha Text Service");
    if (FAILED(hr)) { logFailure(L"CLSID root", hr); return hr; }
    hr = SetRegistryString(HKEY_CLASSES_ROOT, clsidPath + L"\\InprocServer32", L"", modulePath);
    if (FAILED(hr)) { logFailure(L"InprocServer32 default", hr); return hr; }
    hr = SetRegistryString(HKEY_CLASSES_ROOT, clsidPath + L"\\InprocServer32", L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) { logFailure(L"InprocServer32 ThreadingModel", hr); return hr; }

    ITfInputProcessorProfiles* profiles = nullptr;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles));
    if (FAILED(hr))
    {
        logFailure(L"CoCreateInstance ITfInputProcessorProfiles", hr);
        return hr;
    }

    hr = profiles->Register(CLSID_EstraImeTextService);
    if (FAILED(hr))
    {
        logFailure(L"ITfInputProcessorProfiles::Register", hr);
        profiles->Release();
        return hr;
    }

    hr = profiles->AddLanguageProfile(
        CLSID_EstraImeTextService,
        kZhCnLangId,
        GUID_EstraImeProfile,
        L"EstraIME Alpha",
        static_cast<ULONG>(wcslen(L"EstraIME Alpha")),
        modulePath,
        static_cast<ULONG>(wcslen(modulePath)),
        0);
    profiles->Release();
    if (FAILED(hr))
    {
        logFailure(L"AddLanguageProfile", hr);
        return hr;
    }

    hr = RegisterCategories();
    if (FAILED(hr))
    {
        logFailure(L"RegisterCategories", hr);
        return hr;
    }

    hr = RegisterProfile(modulePath);
    if (FAILED(hr))
    {
        logFailure(L"RegisterProfile", hr);
        return hr;
    }

    hr = InstallOrRemoveForCurrentUser(false);
    if (FAILED(hr))
    {
        EstraIme::Common::Logger::Warn(std::format(L"InstallLayoutOrTip did not succeed: 0x{:08X}", static_cast<unsigned>(hr)));
    }

    EstraIme::Common::Logger::Info(L"EstraIME text service registered");
    return S_OK;
}

HRESULT UnregisterServer()
{
    auto logFailure = [](const wchar_t* step, const HRESULT hr) {
        EstraIme::Common::Logger::Error(std::format(L"UnregisterServer failed at {}: 0x{:08X}", step, static_cast<unsigned>(hr)));
    };

    const auto clsid = GuidToString(CLSID_EstraImeTextService);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, (L"CLSID\\" + clsid).c_str());

    InstallOrRemoveForCurrentUser(true);

    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_ITfCategoryMgr, reinterpret_cast<void**>(&categories))))
    {
        categories->UnregisterCategory(CLSID_EstraImeTextService, GUID_TFCAT_TIP_KEYBOARD, CLSID_EstraImeTextService);
        categories->UnregisterCategory(CLSID_EstraImeTextService, GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT, CLSID_EstraImeTextService);
        categories->Release();
    }

    ITfInputProcessorProfiles* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER, IID_ITfInputProcessorProfiles, reinterpret_cast<void**>(&profiles))))
    {
        profiles->RemoveLanguageProfile(
            CLSID_EstraImeTextService,
            kZhCnLangId,
            GUID_EstraImeProfile);
        profiles->Unregister(CLSID_EstraImeTextService);
        profiles->Release();
    }
    else
    {
        logFailure(L"CoCreateInstance ITfInputProcessorProfiles", E_FAIL);
    }

    return S_OK;
}
