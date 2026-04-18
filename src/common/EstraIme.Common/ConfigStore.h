#pragma once

#include "TextUtils.h"
#include "EstraImeShared/CommonTypes.h"

#include <string>

namespace EstraIme::Common
{
    class ConfigStore
    {
    public:
        static EstraIme::ImeConfig Load();
        static void Save(const EstraIme::ImeConfig& config);
        static std::wstring DefaultPath();
    };
}
