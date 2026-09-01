#pragma once

#include <string>
#include <string_view>

namespace cpp_bindings_windows::detail
{
inline auto normalizePortPath(std::wstring_view port) -> std::wstring
{
    if (port.starts_with(L"\\\\.\\"))
    {
        return std::wstring(port);
    }
    if (port.starts_with(L"COM") || port.starts_with(L"com"))
    {
        return L"\\\\.\\" + std::wstring(port);
    }
    return std::wstring(port);
}
} // namespace cpp_bindings_windows::detail
