#pragma once

#include "windows.hpp"

#include <string>
#include <string_view>

namespace cpp_bindings_windows::detail
{
inline auto wideToUtf8(std::wstring_view wide) -> std::string
{
    if (wide.empty())
    {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()),
                                             nullptr, 0, nullptr, nullptr);
    if (required <= 0)
    {
        return {};
    }

    std::string utf8(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(), static_cast<int>(wide.size()), utf8.data(),
                            required, nullptr, nullptr) <= 0)
    {
        return {};
    }
    return utf8;
}
} // namespace cpp_bindings_windows::detail
