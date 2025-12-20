#pragma once

#include <cpp_core/status_codes.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <climits>
#include <string>

namespace cpp_bindings_windows::detail
{
class UniqueHandle
{
  public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE h) : handle_(h)
    {
    }

    UniqueHandle(const UniqueHandle &) = delete;
    auto operator=(const UniqueHandle &) -> UniqueHandle & = delete;

    UniqueHandle(UniqueHandle &&other) noexcept : handle_(other.handle_)
    {
        other.handle_ = nullptr;
    }
    auto operator=(UniqueHandle &&other) noexcept -> UniqueHandle &
    {
        if (this != &other)
        {
            reset(other.release());
        }
        return *this;
    }

    ~UniqueHandle()
    {
        reset(nullptr);
    }

    [[nodiscard]] auto get() const -> HANDLE
    {
        return handle_;
    }

    [[nodiscard]] auto valid() const -> bool
    {
        return handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE;
    }

    auto reset(HANDLE new_handle) -> void
    {
        if (valid())
        {
            CloseHandle(handle_);
        }
        handle_ = new_handle;
    }

    [[nodiscard]] auto release() -> HANDLE
    {
        HANDLE out = handle_;
        handle_ = nullptr;
        return out;
    }

  private:
    HANDLE handle_ = nullptr;
};

template <typename Callback>
inline auto invokeErrorCallback(Callback error_callback, cpp_core::StatusCodes code, const char *message) -> void
{
    if (error_callback != nullptr)
    {
        error_callback(static_cast<int>(code), message);
    }
}

template <typename Ret, typename Callback>
inline auto failMsg(Callback error_callback, cpp_core::StatusCodes code, const char *message) -> Ret
{
    invokeErrorCallback(error_callback, code, message);
    return static_cast<Ret>(code);
}

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

template <typename Ret, typename Callback>
inline auto failWin32(Callback error_callback, cpp_core::StatusCodes code) -> Ret
{
    if (error_callback != nullptr)
    {
        const DWORD err = GetLastError();
        const std::string msg = win32ErrorToString(err);
        error_callback(static_cast<int>(code), msg.c_str());
    }
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

} // namespace cpp_bindings_windows::detail
