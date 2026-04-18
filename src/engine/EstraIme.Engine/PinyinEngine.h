#pragma once

#include "EstraImeShared/CommonTypes.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace EstraIme::Engine
{
    class PinyinEngine
    {
    public:
        PinyinEngine();

        std::vector<EstraIme::Candidate> Query(const std::wstring& composition, std::uint64_t generation, std::size_t pageSize = 9);
        void CommitSelection(const std::wstring& composition, const std::wstring& text);

        static bool IsPinyinKey(wchar_t ch);
        static std::wstring MapPunctuation(wchar_t ch);

    private:
        void LoadSeedLexicon();
        void LoadLearnedFrequencies();
        void SaveLearnedFrequencies() const;
        std::vector<std::wstring> Segment(const std::wstring& normalized) const;
        void AddCandidate(std::vector<EstraIme::Candidate>& output, const std::wstring& text, double score, EstraIme::CandidateSource source, std::uint64_t generation) const;
        void TrackMaxKeyLength(const std::wstring& key);

        std::unordered_map<std::wstring, std::vector<std::pair<std::wstring, int>>> lexicon_;
        std::unordered_map<std::wstring, int> learnedWeights_;
        mutable std::mutex mutex_;
        std::size_t maxKeyLength_{0};
    };
}
