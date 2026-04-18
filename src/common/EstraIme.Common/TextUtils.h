#pragma once

#include <Windows.h>

#include <algorithm>
#include <cwctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace EstraIme::Common
{
    inline std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring output(length, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), output.data(), length);
        return output;
    }

    inline std::string WideToUtf8(const std::wstring& value)
    {
        if (value.empty())
        {
            return {};
        }

        const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        std::string output(length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), output.data(), length, nullptr, nullptr);
        return output;
    }

    inline std::wstring ToLowerAscii(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            if (ch >= L'A' && ch <= L'Z')
            {
                return static_cast<wchar_t>(ch - L'A' + L'a');
            }
            return ch;
        });
        return value;
    }

    inline std::wstring NormalizePinyin(std::wstring value)
    {
        std::wstring normalized;
        normalized.reserve(value.size());
        for (const wchar_t ch : value)
        {
            if ((ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch == L'\'')
            {
                normalized.push_back(static_cast<wchar_t>(std::towlower(ch)));
            }
        }

        return normalized;
    }

    inline std::wstring ReadUtf8File(const std::wstring& path)
    {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open())
        {
            return {};
        }

        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return Utf8ToWide(buffer.str());
    }

    inline void WriteUtf8File(const std::wstring& path, const std::wstring& content)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << WideToUtf8(content);
    }

    inline std::wstring Trim(const std::wstring& value)
    {
        const auto first = std::find_if_not(value.begin(), value.end(), iswspace);
        if (first == value.end())
        {
            return {};
        }
        const auto last = std::find_if_not(value.rbegin(), value.rend(), iswspace).base();
        return std::wstring(first, last);
    }

    inline std::optional<std::wstring> ExtractJsonString(const std::wstring& content, const std::wstring& key)
    {
        const std::wstring pattern = L"\"" + key + L"\"";
        const auto keyPos = content.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return std::nullopt;
        }

        const auto colonPos = content.find(L':', keyPos + pattern.size());
        const auto firstQuote = content.find(L'"', colonPos + 1);
        const auto secondQuote = content.find(L'"', firstQuote + 1);
        if (colonPos == std::wstring::npos || firstQuote == std::wstring::npos || secondQuote == std::wstring::npos)
        {
            return std::nullopt;
        }

        return content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
    }

    inline std::optional<bool> ExtractJsonBool(const std::wstring& content, const std::wstring& key)
    {
        const std::wstring pattern = L"\"" + key + L"\"";
        const auto keyPos = content.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return std::nullopt;
        }

        const auto colonPos = content.find(L':', keyPos + pattern.size());
        if (colonPos == std::wstring::npos)
        {
            return std::nullopt;
        }

        const auto tail = content.substr(colonPos + 1, 16);
        if (tail.find(L"true") != std::wstring::npos)
        {
            return true;
        }
        if (tail.find(L"false") != std::wstring::npos)
        {
            return false;
        }

        return std::nullopt;
    }

    inline std::optional<int> ExtractJsonInt(const std::wstring& content, const std::wstring& key)
    {
        const std::wstring pattern = L"\"" + key + L"\"";
        const auto keyPos = content.find(pattern);
        if (keyPos == std::wstring::npos)
        {
            return std::nullopt;
        }

        const auto colonPos = content.find(L':', keyPos + pattern.size());
        if (colonPos == std::wstring::npos)
        {
            return std::nullopt;
        }

        std::wstring number;
        for (std::size_t i = colonPos + 1; i < content.size(); ++i)
        {
            const auto ch = content[i];
            if ((ch >= L'0' && ch <= L'9') || (ch == L'-' && number.empty()))
            {
                number.push_back(ch);
                continue;
            }

            if (!number.empty())
            {
                break;
            }
        }

        if (number.empty())
        {
            return std::nullopt;
        }

        return std::stoi(number);
    }

    inline std::vector<std::wstring> Split(const std::wstring& value, wchar_t delimiter)
    {
        std::vector<std::wstring> parts;
        std::wstringstream stream(value);
        std::wstring item;
        while (std::getline(stream, item, delimiter))
        {
            parts.push_back(item);
        }
        return parts;
    }
}
