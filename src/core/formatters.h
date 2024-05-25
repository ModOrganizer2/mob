#pragma once

#include "../utility/string.h"

template <>
struct std::formatter<std::wstring, char> : std::formatter<std::string, char> {
    template <class FmtContext>
    FmtContext::iterator format(std::wstring const& s, FmtContext& ctx) const
    {
        return std::formatter<std::string, char>::format(mob::utf16_to_utf8(s), ctx);
    }
};

template <>
struct std::formatter<std::filesystem::path, char>
    : std::formatter<std::basic_string<std::filesystem::path::value_type>, char> {
    template <class FmtContext>
    FmtContext::iterator format(std::filesystem::path const& s, FmtContext& ctx) const
    {
        return std::formatter<std::basic_string<std::filesystem::path::value_type>,
                              char>::format(s.native(), ctx);
    }
};

template <class Enum, class CharT>
    requires std::is_enum_v<Enum>
struct std::formatter<Enum, CharT>
    : std::formatter<std::underlying_type_t<Enum>, CharT> {
    template <class FmtContext>
    FmtContext::iterator format(Enum v, FmtContext& ctx) const
    {
        return std::formatter<std::underlying_type_t<Enum>, CharT>::format(
            static_cast<std::underlying_type_t<Enum>>(v), ctx);
    }
};
