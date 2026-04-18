#include "ConfigStore.h"

#include "Log.h"

#include <ShlObj.h>

#include <filesystem>
#include <format>

namespace
{
    std::wstring EnsureConfigRoot()
    {
        PWSTR programDataRaw = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_DEFAULT, nullptr, &programDataRaw)))
        {
            return L".\\ime-config.json";
        }

        std::wstring root(programDataRaw);
        CoTaskMemFree(programDataRaw);
        std::filesystem::path path = std::filesystem::path(root) / L"EstraIME";
        std::filesystem::create_directories(path);
        return path.wstring();
    }
}

namespace EstraIme::Common
{
    std::wstring ConfigStore::DefaultPath()
    {
        return (std::filesystem::path(EnsureConfigRoot()) / L"ime-config.json").wstring();
    }

    EstraIme::ImeConfig ConfigStore::Load()
    {
        EstraIme::ImeConfig config{};
        const auto path = DefaultPath();
        const auto content = ReadUtf8File(path);
        if (content.empty())
        {
            Logger::Info(L"Config file not found, using defaults");
            return config;
        }

        if (const auto value = ExtractJsonString(content, L"mode_default"))
        {
            config.defaultMode = *value;
        }
        if (const auto value = ExtractJsonBool(content, L"enabled"))
        {
            config.llmEnabled = *value;
        }
        if (const auto value = ExtractJsonString(content, L"provider"))
        {
            config.llmProvider = *value;
        }
        if (const auto value = ExtractJsonString(content, L"model_id"))
        {
            config.localLlm.modelId = *value;
        }
        if (const auto value = ExtractJsonString(content, L"endpoint"))
        {
            config.localLlm.endpoint = *value;
        }
        if (const auto value = ExtractJsonInt(content, L"pause_ms"))
        {
            config.trigger.pauseMs = *value;
        }
        if (const auto value = ExtractJsonInt(content, L"min_chars"))
        {
            config.trigger.minChars = *value;
        }
        if (const auto value = ExtractJsonInt(content, L"min_context_chars"))
        {
            config.trigger.minContextChars = *value;
        }
        if (const auto value = ExtractJsonString(content, L"mode"))
        {
            config.privacyMode = *value;
        }
        if (const auto value = ExtractJsonBool(content, L"cloud_opt_in"))
        {
            config.cloudOptIn = *value;
        }
        if (const auto value = ExtractJsonString(content, L"log_level"))
        {
            config.logLevel = *value;
        }

        return config;
    }

    void ConfigStore::Save(const EstraIme::ImeConfig& config)
    {
        const auto path = DefaultPath();
        const auto json = std::format(
            L"{{\n"
            L"  \"ime\": {{\n"
            L"    \"mode_default\": \"{}\"\n"
            L"  }},\n"
            L"  \"llm\": {{\n"
            L"    \"enabled\": {},\n"
            L"    \"provider\": \"{}\",\n"
            L"    \"local\": {{\n"
            L"      \"model_id\": \"{}\",\n"
            L"      \"endpoint\": \"{}\"\n"
            L"    }},\n"
            L"    \"trigger\": {{\n"
            L"      \"pause_ms\": {},\n"
            L"      \"min_chars\": {},\n"
            L"      \"min_context_chars\": {}\n"
            L"    }}\n"
            L"  }},\n"
            L"  \"privacy\": {{\n"
            L"    \"mode\": \"{}\",\n"
            L"    \"cloud_opt_in\": {}\n"
            L"  }},\n"
            L"  \"diagnostics\": {{\n"
            L"    \"log_level\": \"{}\"\n"
            L"  }}\n"
            L"}}\n",
            config.defaultMode,
            config.llmEnabled ? L"true" : L"false",
            config.llmProvider,
            config.localLlm.modelId,
            config.localLlm.endpoint,
            config.trigger.pauseMs,
            config.trigger.minChars,
            config.trigger.minContextChars,
            config.privacyMode,
            config.cloudOptIn ? L"true" : L"false",
            config.logLevel);

        WriteUtf8File(path, json);
    }
}
