#include "CandidateFusion.h"

#include <algorithm>
#include <set>

namespace EstraIme::Fusion
{
    std::vector<EstraIme::Candidate> CandidateFusion::Fuse(
        const std::vector<EstraIme::Candidate>& traditional,
        const std::vector<EstraIme::Candidate>& asyncLlm,
        const std::size_t pageSize,
        const std::uint64_t generation) const
    {
        std::vector<EstraIme::Candidate> merged = traditional;
        std::set<std::wstring> seen;
        for (const auto& item : traditional)
        {
            seen.insert(item.text);
        }

        for (auto candidate : asyncLlm)
        {
            if (candidate.generation != generation || candidate.text.empty() || seen.contains(candidate.text))
            {
                continue;
            }

            candidate.source = EstraIme::CandidateSource::Llm;
            candidate.annotation = L"llm";

            const auto insertAt = merged.empty() ? 0 : 1;
            merged.insert(merged.begin() + static_cast<std::ptrdiff_t>(insertAt), candidate);
            seen.insert(candidate.text);
        }

        if (merged.size() > pageSize)
        {
            merged.resize(pageSize);
        }

        return merged;
    }
}
