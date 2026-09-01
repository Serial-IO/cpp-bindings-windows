#pragma once

#include "windows.hpp"

#include <string>

namespace cpp_bindings_windows::detail
{
inline auto win32ErrorToString(DWORD error) -> std::string
{
    LPSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD language_id = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const DWORD length =
        FormatMessageA(flags, nullptr, error, language_id, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    if (length == 0 || buffer == nullptr)
    {
        return "Unknown Win32 error (" + std::to_string(error) + ")";
    }

    std::string message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n'))
    {
        message.pop_back();
    }
    return message;
}
} // namespace cpp_bindings_windows::detail
