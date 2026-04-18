#include "IpcClient.h"

#include "Log.h"
#include "PathUtils.h"
#include "TextUtils.h"

#include <Windows.h>

#include <format>
#include <string>
#include <thread>

namespace
{
    constexpr wchar_t kPipeName[] = LR"(\\.\pipe\LOCAL\EstraIME.Sidecar)";

    std::wstring SerializeRequest(const EstraIme::AutocompleteRequest& request)
    {
        return std::format(
            L"{{\"method\":\"autocomplete.request\",\"session_id\":\"{}\",\"request_id\":\"{}\",\"generation_id\":{},\"context_before\":\"{}\",\"composition_text\":\"{}\",\"caret_caps\":{{\"supports_ui\":{},\"secure_input\":{}}}}}",
            request.sessionId,
            request.requestId,
            request.generationId,
            request.contextBefore,
            request.compositionText,
            request.supportsUi ? L"true" : L"false",
            request.secureInput ? L"true" : L"false");
    }

    EstraIme::AutocompleteResponse ParseResponse(const std::wstring& content)
    {
        EstraIme::AutocompleteResponse response{};
        response.ok = !content.empty();

        if (const auto requestId = EstraIme::Common::ExtractJsonString(content, L"request_id"))
        {
            response.requestId = *requestId;
        }
        if (const auto generation = EstraIme::Common::ExtractJsonInt(content, L"generation_id"))
        {
            response.generationId = static_cast<std::uint64_t>(*generation);
        }

        std::size_t cursor = 0;
        while ((cursor = content.find(L"\"text\"", cursor)) != std::wstring::npos)
        {
            const auto start = content.find(L'"', content.find(L':', cursor) + 1);
            const auto end = content.find(L'"', start + 1);
            if (start == std::wstring::npos || end == std::wstring::npos)
            {
                break;
            }

            EstraIme::Candidate candidate{};
            candidate.text = content.substr(start + 1, end - start - 1);
            candidate.source = EstraIme::CandidateSource::Llm;
            candidate.score = 0.8;
            candidate.annotation = L"llm";
            response.suggestions.push_back(std::move(candidate));
            cursor = end + 1;
        }

        return response;
    }

    bool LaunchSidecarProcess()
    {
        const auto path = EstraIme::Common::ResolveSidecarExecutablePath(reinterpret_cast<const void*>(&LaunchSidecarProcess));
        if (!path)
        {
            EstraIme::Common::Logger::Warn(L"Sidecar executable could not be resolved");
            return false;
        }

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);

        PROCESS_INFORMATION processInfo{};
        std::wstring commandLine = L"\"" + *path + L"\"";

        const auto created = CreateProcessW(
            nullptr,
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            DETACHED_PROCESS | CREATE_NO_WINDOW,
            nullptr,
            std::filesystem::path(*path).parent_path().c_str(),
            &startupInfo,
            &processInfo);

        if (!created)
        {
            EstraIme::Common::Logger::Warn(L"Failed to launch sidecar process");
            return false;
        }

        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        EstraIme::Common::Logger::Info(L"Sidecar process launched");
        return true;
    }
}

namespace EstraIme::Common
{
    void SidecarClient::RequestAutocompleteAsync(const EstraIme::AutocompleteRequest& request, Callback callback) const
    {
        std::thread([request, callback]() {
            const auto response = SendRequest(request);
            callback(response);
        }).detach();
    }

    bool SidecarClient::IsHealthy() const
    {
        return TryEnsurePipeReady();
    }

    EstraIme::AutocompleteResponse SidecarClient::SendRequest(const EstraIme::AutocompleteRequest& request)
    {
        EstraIme::AutocompleteResponse fallback{};
        fallback.requestId = request.requestId;
        fallback.generationId = request.generationId;

        if (request.secureInput)
        {
            return fallback;
        }

        if (!TryEnsurePipeReady())
        {
            Logger::Warn(L"Sidecar pipe unavailable; returning empty autocomplete set");
            return fallback;
        }

        HANDLE pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            Logger::Warn(L"Failed to connect to sidecar pipe");
            return fallback;
        }

        const auto payloadUtf8 = WideToUtf8(SerializeRequest(request));
        const auto payloadLength = static_cast<std::uint32_t>(payloadUtf8.size());

        DWORD written = 0;
        WriteFile(pipe, &payloadLength, sizeof(payloadLength), &written, nullptr);
        WriteFile(pipe, payloadUtf8.data(), payloadLength, &written, nullptr);

        std::uint32_t responseLength = 0;
        DWORD read = 0;
        if (!ReadFile(pipe, &responseLength, sizeof(responseLength), &read, nullptr) || read != sizeof(responseLength))
        {
            CloseHandle(pipe);
            return fallback;
        }

        std::string responseUtf8(responseLength, '\0');
        if (!ReadFile(pipe, responseUtf8.data(), responseLength, &read, nullptr))
        {
            CloseHandle(pipe);
            return fallback;
        }

        CloseHandle(pipe);
        return ParseResponse(Utf8ToWide(responseUtf8));
    }

    bool SidecarClient::TryEnsurePipeReady()
    {
        if (WaitNamedPipeW(kPipeName, 50))
        {
            return true;
        }

        if (!LaunchSidecarProcess())
        {
            return false;
        }

        for (int attempt = 0; attempt < 20; ++attempt)
        {
            if (WaitNamedPipeW(kPipeName, 100))
            {
                return true;
            }
            Sleep(100);
        }

        Logger::Warn(L"Sidecar launch timed out waiting for pipe");
        return false;
    }
}
