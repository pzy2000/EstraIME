#pragma once

#include <Windows.h>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace EstraIme::Common
{
    inline std::optional<std::wstring> GetEnvironmentPath(const wchar_t* name)
    {
        std::wstring value(512, L'\0');
        size_t required = 0;
        if (_wgetenv_s(&required, value.data(), value.size(), name) != 0 || required <= 1)
        {
            return std::nullopt;
        }

        value.resize(required - 1);
        return value;
    }

    inline std::filesystem::path ModulePathFromAddress(const void* address)
    {
        HMODULE module = nullptr;
        if (!GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &module))
        {
            return {};
        }

        wchar_t buffer[MAX_PATH]{};
        GetModuleFileNameW(module, buffer, static_cast<DWORD>(std::size(buffer)));
        return buffer;
    }

    inline std::vector<std::filesystem::path> ProbeRootsFromAddress(const void* address)
    {
        std::vector<std::filesystem::path> roots;
        auto modulePath = ModulePathFromAddress(address);
        if (modulePath.empty())
        {
            return roots;
        }

        auto current = modulePath.parent_path();
        for (int depth = 0; depth < 8 && !current.empty(); ++depth)
        {
            roots.push_back(current);
            current = current.parent_path();
        }

        return roots;
    }

    inline std::optional<std::wstring> ResolveExistingPath(
        const void* address,
        const std::optional<std::wstring>& environmentOverride,
        const std::vector<std::filesystem::path>& relatives)
    {
        if (environmentOverride)
        {
            const std::filesystem::path direct(*environmentOverride);
            if (std::filesystem::exists(direct))
            {
                return direct.wstring();
            }
        }

        for (const auto& root : ProbeRootsFromAddress(address))
        {
            for (const auto& relative : relatives)
            {
                const auto candidate = root / relative;
                if (std::filesystem::exists(candidate))
                {
                    return candidate.wstring();
                }
            }
        }

        return std::nullopt;
    }

    inline std::optional<std::wstring> ResolveDataFilePath(const void* address, const std::wstring& fileName)
    {
        const auto dataDir = GetEnvironmentPath(L"ESTRAIME_DATA_DIR");
        return ResolveExistingPath(
            address,
            dataDir,
            {
                std::filesystem::path(L"data") / L"generated" / fileName,
                std::filesystem::path(L"data") / fileName,
                std::filesystem::path(L"generated") / fileName
            });
    }

    inline std::optional<std::wstring> ResolveSidecarExecutablePath(const void* address, const std::wstring& configuration = L"Debug")
    {
        const auto configured = GetEnvironmentPath(L"ESTRAIME_SIDECAR_EXE");
        const auto lowerConfiguration = configuration == L"Release" ? L"release" : L"debug";
        return ResolveExistingPath(
            address,
            configured,
            {
                L"sidecar-daemon.exe",
                std::filesystem::path(L"sidecar") / L"target" / lowerConfiguration / L"sidecar-daemon.exe",
                std::filesystem::path(L"sidecar") / L"target" / L"debug" / L"sidecar-daemon.exe",
                std::filesystem::path(L"sidecar") / L"target" / L"release" / L"sidecar-daemon.exe"
            });
    }
}
