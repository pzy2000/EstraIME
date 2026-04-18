#include "../../src/engine/EstraIme.Engine/PinyinEngine.h"
#include "../../src/fusion/EstraIme.Fusion/CandidateFusion.h"

#include <iostream>

int wmain()
{
    EstraIme::Engine::PinyinEngine engine;
    const auto candidates = engine.Query(L"nihao", 1, 9);
    if (candidates.empty() || candidates.front().text != L"\u4F60\u597D")
    {
        std::wcerr << L"expected \\u4F60\\u597D as first candidate\n";
        return 1;
    }

    std::vector<EstraIme::Candidate> llm{
        {.text = L"\u4F60\u597D\u4E16\u754C", .source = EstraIme::CandidateSource::Llm, .score = 0.88, .annotation = L"llm", .generation = 2}
    };

    EstraIme::Fusion::CandidateFusion fusion;
    const auto merged = fusion.Fuse(candidates, llm, 9, 2);
    if (merged.size() < 2 || merged[1].text != L"\u4F60\u597D\u4E16\u754C")
    {
        std::wcerr << L"expected llm suggestion near the front\n";
        return 1;
    }

    if (EstraIme::Engine::PinyinEngine::MapPunctuation(L'.') != L"\u3002")
    {
        std::wcerr << L"punctuation mapping failed\n";
        return 1;
    }

    const auto prefixCandidates = engine.Query(L"shuru", 3, 32);
    bool hasInputMethod = false;
    for (const auto& candidate : prefixCandidates)
    {
        if (candidate.text == L"\u8F93\u5165\u6CD5")
        {
            hasInputMethod = true;
            break;
        }
    }
    if (!hasInputMethod)
    {
        std::wcerr << L"expected prefix candidate for \\u8F93\\u5165\\u6CD5\n";
        return 1;
    }

    std::wcout << L"engine smoke tests passed\n";
    return 0;
}
