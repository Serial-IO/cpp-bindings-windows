#pragma once

#include <cpp_core/error_handling.hpp>
#include <cpp_core/scope_guard.hpp>
#include <cpp_core/unique_resource.hpp>
#include <cpp_core/validation.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <climits>
#include <cstdint>
#include <limits>
#include <string>

namespace cpp_bindings_windows::detail
{

// Win32 HANDLE traits for UniqueResource
struct Win32HandleTraits
{
    using handle_type = HANDLE;

    static constexpr auto invalid() noexcept -> handle_type
    {
        return nullptr;
    }

    static auto close(handle_type h) noexcept -> void
    {
        if (h != INVALID_HANDLE_VALUE)
        {
            CloseHandle(h);
        }
    }
};

using UniqueHandle = cpp_core::UniqueResource<Win32HandleTraits>;

// Win32-specific error helpers
inline auto win32ErrorToString(DWORD err) -> std::string
{
    LPSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD lang_id = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);

    const DWORD len = FormatMessageA(flags, nullptr, err, lang_id, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);
    if (len == 0 || buffer == nullptr)
    {
        return "Unknown Win32 error";
    }

    std::string msg(buffer, len);
    LocalFree(buffer);

    while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n'))
    {
        msg.pop_back();
    }
    return msg;
}

template <cpp_core::StatusConvertible Ret, cpp_core::ErrorCallback Callback>
inline auto failWin32(Callback &&error_callback, cpp_core::StatusCodes code) -> Ret
{
    const DWORD err = GetLastError();
    const std::string msg = win32ErrorToString(err);
    cpp_core::invokeError(std::forward<Callback>(error_callback), code, msg);
    return static_cast<Ret>(code);
}

inline auto bytesWaiting(HANDLE handle, int *out_bytes) -> bool
{
    if (out_bytes == nullptr)
    {
        return false;
    }
    *out_bytes = 0;

    DWORD errors = 0;
    COMSTAT stat = {};
    if (ClearCommError(handle, &errors, &stat) == 0)
    {
        return false;
    }

    if (stat.cbInQue > static_cast<DWORD>(INT_MAX))
    {
        *out_bytes = INT_MAX;
    }
    else
    {
        *out_bytes = static_cast<int>(stat.cbInQue);
    }
    return true;
}

// Combined int64_t -> HANDLE validation for the C API boundary.
// Checks numeric range, nullptr, and INVALID_HANDLE_VALUE.
template <cpp_core::StatusConvertible Ret, cpp_core::ErrorCallback Callback>
inline auto validateWin32Handle(int64_t handle, Callback &&error_callback, HANDLE *out) -> Ret
{
    if (handle <= 0 || handle > std::numeric_limits<int>::max() || handle > std::numeric_limits<intptr_t>::max())
    {
        return cpp_core::failMsg<Ret>(std::forward<Callback>(error_callback),
                                      cpp_core::StatusCodes::kInvalidHandleError, "Invalid handle");
    }
    const HANDLE h = reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
    if (h == nullptr || h == INVALID_HANDLE_VALUE)
    {
        return cpp_core::failMsg<Ret>(std::forward<Callback>(error_callback),
                                      cpp_core::StatusCodes::kInvalidHandleError, "Invalid handle");
    }
    *out = h;
    return static_cast<Ret>(cpp_core::StatusCodes::kSuccess);
}

} // namespace cpp_bindings_windows::detail
