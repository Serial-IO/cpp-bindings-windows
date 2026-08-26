#pragma once

#include "common_types.hpp"
#include "handle_state.hpp"

#include <cpp_core/error_handling.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <climits>
#include <string>
#include <string_view>
#include <utility>

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

template <cpp_core::StatusConvertible ReturnType>
inline auto failWin32(ErrorCallbackT error_callback, StatusCodeValue code) -> ReturnType
{
    const DWORD error = GetLastError();
    const std::string message = win32ErrorToString(error);
    cpp_core::invokeError(effectiveErrorCallback(error_callback), code, message);
    return static_cast<ReturnType>(code);
}

inline auto bytesWaiting(HANDLE handle, int *out_bytes) -> bool
{
    if (out_bytes == nullptr)
    {
        return false;
    }
    *out_bytes = 0;

    DWORD errors = 0;
    COMSTAT status = {};
    if (ClearCommError(handle, &errors, &status) == 0)
    {
        return false;
    }

    *out_bytes = status.cbInQue > static_cast<DWORD>(INT_MAX) ? INT_MAX : static_cast<int>(status.cbInQue);
    return true;
}

} // namespace cpp_bindings_windows::detail
