#pragma once

#include "EstraImeShared/CommonTypes.h"

#include <functional>

namespace EstraIme::Common
{
    class SidecarClient
    {
    public:
        using Callback = std::function<void(const EstraIme::AutocompleteResponse&)>;

        void RequestAutocompleteAsync(const EstraIme::AutocompleteRequest& request, Callback callback) const;
        bool IsHealthy() const;
        std::wstring GetHealthSummary() const;

    private:
        static EstraIme::AutocompleteResponse SendRequest(const EstraIme::AutocompleteRequest& request);
        static std::wstring SendJsonPayload(const std::wstring& payload);
        static bool TryEnsurePipeReady();
    };
}
