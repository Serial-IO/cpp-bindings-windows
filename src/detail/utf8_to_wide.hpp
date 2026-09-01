#pragma once

#include "windows.hpp"

#include <string>

namespace cpp_bindings_windows::detail
{
inline auto utf8ToWide(const char *utf8) -> std::wstring
{
    if (utf8 == nullptr || *utf8 == '\0')
    {
        return {};
    }

    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (required <= 0)
    {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(required), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, wide.data(), required) <= 0)
    {
        return {};
    }
    wide.pop_back();
    return wide;
}
} // namespace cpp_bindings_windows::detail
