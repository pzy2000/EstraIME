#include "Log.h"

#include "TextUtils.h"

#include <ShlObj.h>
#include <Windows.h>

#include <filesystem>
#include <format>

namespace
{
    std::wstring GetLogPath()
    {
        PWSTR appDataRaw = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appDataRaw)))
        {
            return L"EstraIME.log";
        }

        std::wstring root(appDataRaw);
        CoTaskMemFree(appDataRaw);

        std::filesystem::path dir = std::filesystem::path(root) / L"EstraIME" / L"logs";
        std::filesystem::create_directories(dir);
        return (dir / L"estraime.log").wstring();
    }
}

namespace EstraIme::Common
{
    void Logger::Info(const std::wstring& message)
    {
        WriteLine(L"INFO", message);
    }

    void Logger::Warn(const std::wstring& message)
    {
        WriteLine(L"WARN", message);
    }

    void Logger::Error(const std::wstring& message)
    {
        WriteLine(L"ERROR", message);
    }

    void Logger::WriteLine(const std::wstring& level, const std::wstring& message)
    {
        SYSTEMTIME now{};
        GetLocalTime(&now);

        const auto line = std::format(
            L"{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{:03} [{}] {}\r\n",
            now.wYear,
            now.wMonth,
            now.wDay,
            now.wHour,
            now.wMinute,
            now.wSecond,
            now.wMilliseconds,
            level,
            message);

        WriteUtf8File(GetLogPath(), ReadUtf8File(GetLogPath()) + line);
        OutputDebugStringW(line.c_str());
    }
}
