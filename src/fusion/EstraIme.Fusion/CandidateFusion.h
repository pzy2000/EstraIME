#pragma once

#include "EstraImeShared/CommonTypes.h"

#include <vector>

namespace EstraIme::Fusion
{
    class CandidateFusion
    {
    public:
        std::vector<EstraIme::Candidate> Fuse(
            const std::vector<EstraIme::Candidate>& traditional,
            const std::vector<EstraIme::Candidate>& asyncLlm,
            std::size_t pageSize,
            std::uint64_t generation) const;
    };
}
