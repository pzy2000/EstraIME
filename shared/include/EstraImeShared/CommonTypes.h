#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace EstraIme
{
    enum class CandidateSource
    {
        Traditional,
        Learned,
        Llm
    };

    struct Candidate
    {
        std::wstring text;
        CandidateSource source{CandidateSource::Traditional};
        double score{0.0};
        std::wstring annotation;
        std::uint32_t segmentStart{0};
        std::uint32_t segmentEnd{0};
        std::uint64_t generation{0};
    };

    struct TriggerConfig
    {
        bool enabled{true};
        int pauseMs{180};
        int minChars{4};
        int minContextChars{8};
    };

    struct LocalLlmConfig
    {
        std::wstring modelId{L"qwen3-1.7b-q4"};
        std::wstring endpoint{L"http://127.0.0.1:8080/v1/chat/completions"};
    };

    struct ImeConfig
    {
        std::wstring defaultMode{L"chinese"};
        bool llmEnabled{true};
        std::wstring llmProvider{L"mock"};
        LocalLlmConfig localLlm{};
        TriggerConfig trigger{};
        std::wstring privacyMode{L"strict"};
        bool cloudOptIn{false};
        std::wstring logLevel{L"info"};
    };

    struct AutocompleteRequest
    {
        std::wstring sessionId;
        std::wstring requestId;
        std::uint64_t generationId{0};
        std::wstring contextBefore;
        std::wstring compositionText;
        bool supportsUi{true};
        bool secureInput{false};
    };

    struct AutocompleteResponse
    {
        std::wstring requestId;
        std::uint64_t generationId{0};
        bool ok{false};
        std::vector<Candidate> suggestions;
    };
}
