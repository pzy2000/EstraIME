#include "PinyinEngine.h"

#include "../../common/EstraIme.Common/PathUtils.h"
#include "../../common/EstraIme.Common/TextUtils.h"

#include <ShlObj.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <limits>
#include <set>

namespace
{
    const std::vector<std::tuple<std::wstring, std::wstring, int>> kFallbackEntries{
        {L"ni", L"\u4F60", 1000},
        {L"ni", L"\u5462", 150},
        {L"hao", L"\u597D", 980},
        {L"nihao", L"\u4F60\u597D", 2200},
        {L"wo", L"\u6211", 1200},
        {L"women", L"\u6211\u4EEC", 1600},
        {L"shi", L"\u662F", 1500},
        {L"shijie", L"\u4E16\u754C", 1400},
        {L"zhongguo", L"\u4E2D\u56FD", 1800},
        {L"zhongwen", L"\u4E2D\u6587", 1400},
        {L"shuru", L"\u8F93\u5165", 1200},
        {L"zhinengshuru", L"\u667A\u80FD\u8F93\u5165", 820},
        {L"gongcheng", L"\u5DE5\u7A0B", 900},
    };

    std::wstring GetLearnedPath()
    {
        PWSTR appDataRaw = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &appDataRaw)))
        {
            return L".\\learned.tsv";
        }

        std::wstring root(appDataRaw);
        CoTaskMemFree(appDataRaw);
        std::filesystem::path path = std::filesystem::path(root) / L"EstraIME";
        std::filesystem::create_directories(path);
        return (path / L"learned.tsv").wstring();
    }
}

namespace EstraIme::Engine
{
    PinyinEngine::PinyinEngine()
    {
        LoadSeedLexicon();
        LoadLearnedFrequencies();
    }

    std::vector<EstraIme::Candidate> PinyinEngine::Query(const std::wstring& composition, std::uint64_t generation, std::size_t pageSize)
    {
        std::vector<EstraIme::Candidate> output;
        const auto normalized = Common::NormalizePinyin(composition);
        if (normalized.empty())
        {
            return output;
        }

        const auto direct = lexicon_.find(normalized);
        if (direct != lexicon_.end())
        {
            for (const auto& [text, baseWeight] : direct->second)
            {
                const auto key = normalized + L"\t" + text;
                const auto learned = learnedWeights_.contains(key) ? learnedWeights_.at(key) : 0;
                const auto score = static_cast<double>(baseWeight + learned * 50);
                AddCandidate(output, text, score, learned > 0 ? EstraIme::CandidateSource::Learned : EstraIme::CandidateSource::Traditional, generation);
            }
        }

        std::vector<std::pair<std::wstring, int>> prefixMatches;
        for (const auto& [key, entries] : lexicon_)
        {
            if (key == normalized || !key.starts_with(normalized) || entries.empty())
            {
                continue;
            }

            prefixMatches.push_back(entries.front());
        }

        std::sort(prefixMatches.begin(), prefixMatches.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.second == rhs.second)
            {
                return lhs.first < rhs.first;
            }
            return lhs.second > rhs.second;
        });

        const auto prefixBudget = std::min<std::size_t>(12, prefixMatches.size());
        for (std::size_t index = 0; index < prefixBudget; ++index)
        {
            AddCandidate(output, prefixMatches[index].first, static_cast<double>(prefixMatches[index].second) - 25.0, EstraIme::CandidateSource::Traditional, generation);
        }

        const auto segments = Segment(normalized);
        if (segments.size() > 1)
        {
            std::wstring joined;
            for (const auto& segment : segments)
            {
                const auto hit = lexicon_.find(segment);
                if (hit != lexicon_.end() && !hit->second.empty())
                {
                    joined += hit->second.front().first;
                }
            }

            if (!joined.empty())
            {
                AddCandidate(output, joined, 640.0, EstraIme::CandidateSource::Traditional, generation);
            }
        }

        AddCandidate(output, normalized, 50.0, EstraIme::CandidateSource::Traditional, generation);

        std::sort(output.begin(), output.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.score == rhs.score)
            {
                return lhs.text < rhs.text;
            }
            return lhs.score > rhs.score;
        });

        output.erase(std::unique(output.begin(), output.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.text == rhs.text;
        }), output.end());

        if (output.size() > pageSize)
        {
            output.resize(pageSize);
        }

        return output;
    }

    void PinyinEngine::CommitSelection(const std::wstring& composition, const std::wstring& text)
    {
        const auto normalized = Common::NormalizePinyin(composition);
        if (normalized.empty() || text.empty())
        {
            return;
        }

        std::scoped_lock lock(mutex_);
        learnedWeights_[normalized + L"\t" + text] += 1;
        SaveLearnedFrequencies();
    }

    bool PinyinEngine::IsPinyinKey(const wchar_t ch)
    {
        return (ch >= L'a' && ch <= L'z') || (ch >= L'A' && ch <= L'Z') || ch == L'\'';
    }

    std::wstring PinyinEngine::MapPunctuation(const wchar_t ch)
    {
        switch (ch)
        {
        case L',':
            return L"\uFF0C";
        case L'.':
            return L"\u3002";
        case L';':
            return L"\uFF1B";
        case L':':
            return L"\uFF1A";
        case L'?':
            return L"\uFF1F";
        case L'!':
            return L"\uFF01";
        default:
            return std::wstring(1, ch);
        }
    }

    void PinyinEngine::LoadSeedLexicon()
    {
        for (const auto& [pinyin, text, weight] : kFallbackEntries)
        {
            lexicon_[pinyin].push_back({text, weight});
            TrackMaxKeyLength(pinyin);
        }

        const auto candidate = Common::ResolveDataFilePath(reinterpret_cast<const void*>(&GetLearnedPath), L"base_lexicon.tsv");
        if (!candidate)
        {
            return;
        }

        std::wifstream stream(*candidate);
        stream.imbue(std::locale(".UTF-8"));
        std::wstring line;
        while (std::getline(stream, line))
        {
            if (line.empty())
            {
                continue;
            }

            const auto parts = Common::Split(line, L'\t');
            if (parts.size() < 3)
            {
                continue;
            }

            lexicon_[parts[0]].push_back({parts[1], std::stoi(parts[2])});
            TrackMaxKeyLength(parts[0]);
        }
    }

    void PinyinEngine::LoadLearnedFrequencies()
    {
        std::wifstream stream(GetLearnedPath());
        stream.imbue(std::locale(".UTF-8"));
        if (!stream.is_open())
        {
            return;
        }

        std::wstring line;
        while (std::getline(stream, line))
        {
            const auto parts = Common::Split(line, L'\t');
            if (parts.size() == 3)
            {
                learnedWeights_[parts[0] + L"\t" + parts[1]] = std::stoi(parts[2]);
            }
        }
    }

    void PinyinEngine::SaveLearnedFrequencies() const
    {
        std::wofstream stream(GetLearnedPath(), std::ios::trunc);
        stream.imbue(std::locale(".UTF-8"));
        for (const auto& [key, value] : learnedWeights_)
        {
            const auto separator = key.find(L'\t');
            if (separator == std::wstring::npos)
            {
                continue;
            }
            stream << key.substr(0, separator) << L'\t' << key.substr(separator + 1) << L'\t' << value << L'\n';
        }
    }

    std::vector<std::wstring> PinyinEngine::Segment(const std::wstring& normalized) const
    {
        struct PathNode
        {
            bool valid{false};
            double score{std::numeric_limits<double>::lowest()};
            std::vector<std::wstring> segments;
        };

        if (!normalized.empty() && maxKeyLength_ > 0)
        {
            std::vector<PathNode> dp(normalized.size() + 1);
            dp[0].valid = true;
            dp[0].score = 0.0;

            for (std::size_t index = 0; index < normalized.size(); ++index)
            {
                if (!dp[index].valid)
                {
                    continue;
                }

                const auto remaining = normalized.size() - index;
                const auto maxLength = std::min(maxKeyLength_, remaining);
                for (std::size_t length = 1; length <= maxLength; ++length)
                {
                    const auto slice = normalized.substr(index, length);
                    const auto hit = lexicon_.find(slice);
                    if (hit == lexicon_.end() || hit->second.empty())
                    {
                        continue;
                    }

                    const auto nextIndex = index + length;
                    const auto nextScore = dp[index].score + hit->second.front().second - static_cast<double>(length);
                    if (!dp[nextIndex].valid || nextScore > dp[nextIndex].score)
                    {
                        dp[nextIndex].valid = true;
                        dp[nextIndex].score = nextScore;
                        dp[nextIndex].segments = dp[index].segments;
                        dp[nextIndex].segments.push_back(slice);
                    }
                }
            }

            if (dp[normalized.size()].valid && dp[normalized.size()].segments.size() > 1)
            {
                return dp[normalized.size()].segments;
            }
        }

        static const std::set<std::wstring> known{
            L"ni", L"hao", L"wo", L"men", L"shi", L"zhong", L"guo", L"wen", L"shu", L"ru", L"gong", L"cheng", L"zhi", L"neng", L"shi", L"jie"
        };

        std::vector<std::wstring> result;
        std::size_t index = 0;
        while (index < normalized.size())
        {
            std::wstring hit;
            for (std::size_t length = std::min<std::size_t>(6, normalized.size() - index); length >= 1; --length)
            {
                const auto slice = normalized.substr(index, length);
                if (known.contains(slice))
                {
                    hit = slice;
                    break;
                }
                if (length == 1)
                {
                    break;
                }
            }

            if (hit.empty())
            {
                break;
            }

            result.push_back(hit);
            index += hit.size();
        }

        return result;
    }

    void PinyinEngine::AddCandidate(std::vector<EstraIme::Candidate>& output, const std::wstring& text, const double score, const EstraIme::CandidateSource source, const std::uint64_t generation) const
    {
        output.push_back({
            .text = text,
            .source = source,
            .score = score,
            .annotation = source == EstraIme::CandidateSource::Llm ? L"llm" : L"",
            .segmentStart = 0,
            .segmentEnd = 0,
            .generation = generation
        });
    }

    void PinyinEngine::TrackMaxKeyLength(const std::wstring& key)
    {
        maxKeyLength_ = std::max(maxKeyLength_, key.size());
    }
}
