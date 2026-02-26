#include <cpp_core/interface/serial_read.h>
#include <cpp_core/scope_guard.hpp>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

#include <algorithm>

namespace
{
auto waitForRxChar(HANDLE handle, int timeout_ms) -> int
{
    timeout_ms = cpp_core::clampTimeout(timeout_ms);

    if (SetCommMask(handle, EV_RXCHAR) == 0)
    {
        return -1;
    }

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ov.hEvent == nullptr)
    {
        return -1;
    }
    DEFER
    {
        CloseHandle(ov.hEvent);
    };

    DWORD mask = 0;
    const BOOL ok = WaitCommEvent(handle, &mask, &ov);
    if (ok != 0)
    {
        return 1;
    }
    if (ok == 0)
    {
        const DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING)
        {
            SetLastError(err);
            return -1;
        }
    }

    const DWORD wait_rc = WaitForSingleObject(ov.hEvent, static_cast<DWORD>(timeout_ms));
    if (wait_rc == WAIT_TIMEOUT)
    {
        CancelIoEx(handle, &ov);
        return 0;
    }
    if (wait_rc != WAIT_OBJECT_0)
    {
        CancelIoEx(handle, &ov);
        SetLastError(ERROR_GEN_FAILURE);
        return -1;
    }

    DWORD bytes = 0;
    if (GetOverlappedResult(handle, &ov, &bytes, FALSE) == 0)
    {
        return -1;
    }

    return 1;
}

auto readSome(HANDLE handle, unsigned char *dst, int size, int timeout_ms) -> int
{
    if (size <= 0)
    {
        return 0;
    }

    OVERLAPPED ov = {};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (ov.hEvent == nullptr)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return -1;
    }
    DEFER
    {
        CloseHandle(ov.hEvent);
    };

    DWORD bytes_read = 0;
    const BOOL ok = ReadFile(handle, dst, static_cast<DWORD>(size), &bytes_read, &ov);
    if (ok != 0)
    {
        return static_cast<int>(bytes_read);
    }

    const DWORD err = GetLastError();
    if (err != ERROR_IO_PENDING)
    {
        SetLastError(err);
        return -1;
    }

    timeout_ms = cpp_core::clampTimeout(timeout_ms);
    const DWORD wait_rc = WaitForSingleObject(ov.hEvent, static_cast<DWORD>(timeout_ms));
    if (wait_rc == WAIT_TIMEOUT)
    {
        CancelIoEx(handle, &ov);
        return 0;
    }
    if (wait_rc != WAIT_OBJECT_0)
    {
        CancelIoEx(handle, &ov);
        SetLastError(ERROR_GEN_FAILURE);
        return -1;
    }

    if (GetOverlappedResult(handle, &ov, &bytes_read, FALSE) == 0)
    {
        return -1;
    }

    return static_cast<int>(bytes_read);
}
} // namespace

extern "C"
{
    MODULE_API auto serialRead(int64_t handle, void *buffer, int buffer_size, int timeout_ms, int /*multiplier*/,
                               ErrorCallbackT error_callback) -> int
    {
        const auto buf_ok = cpp_core::validateBuffer<int>(buffer, buffer_size, error_callback);
        if (buf_ok < 0)
        {
            return buf_ok;
        }

        HANDLE h = nullptr;
        const auto handle_ok =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (handle_ok < 0)
        {
            return handle_ok;
        }

        auto *buf = static_cast<unsigned char *>(buffer);

        int waiting = 0;
        if (!cpp_bindings_windows::detail::bytesWaiting(h, &waiting))
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        if (waiting <= 0)
        {
            if (timeout_ms <= 0)
            {
                return 0;
            }
            const int ready = waitForRxChar(h, timeout_ms);
            if (ready < 0)
            {
                return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kReadError);
            }
            if (ready == 0)
            {
                return 0;
            }
        }

        if (!cpp_bindings_windows::detail::bytesWaiting(h, &waiting))
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        if (waiting <= 0)
        {
            return 0;
        }

        const int first_chunk = std::min(waiting, buffer_size);
        int total = readSome(h, buf, first_chunk, timeout_ms);
        if (total < 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kReadError);
        }
        if (total == 0)
        {
            total = readSome(h, buf, first_chunk, 10);
            if (total < 0)
            {
                return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kReadError);
            }
            if (total == 0)
            {
                return 0;
            }
        }

        while (total < buffer_size)
        {
            if (!cpp_bindings_windows::detail::bytesWaiting(h, &waiting))
            {
                return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                    cpp_core::StatusCodes::kGetStateError);
            }
            if (waiting <= 0)
            {
                break;
            }
            const int chunk = std::min(waiting, buffer_size - total);
            const int got = readSome(h, buf + total, chunk, 0);
            if (got <= 0)
            {
                break;
            }
            total += got;
        }

        return total;
    }

} // extern "C"
