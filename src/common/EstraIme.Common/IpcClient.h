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

    private:
        static EstraIme::AutocompleteResponse SendRequest(const EstraIme::AutocompleteRequest& request);
        static bool TryEnsurePipeReady();
    };
}
