#pragma once

#include <string>

namespace EstraIme::Common
{
    class Logger
    {
    public:
        static void Info(const std::wstring& message);
        static void Warn(const std::wstring& message);
        static void Error(const std::wstring& message);

    private:
        static void WriteLine(const std::wstring& level, const std::wstring& message);
    };
}
