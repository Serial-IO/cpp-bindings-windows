#include <cpp_core/interface/serial_write.h>
#include <cpp_core/scope_guard.hpp>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

namespace
{
auto writeSome(HANDLE handle, const void *src, int size, int timeout_ms) -> int
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

    DWORD bytes_written = 0;
    const BOOL ok = WriteFile(handle, src, static_cast<DWORD>(size), &bytes_written, &ov);
    if (ok != 0)
    {
        return static_cast<int>(bytes_written);
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

    if (GetOverlappedResult(handle, &ov, &bytes_written, FALSE) == 0)
    {
        return -1;
    }

    return static_cast<int>(bytes_written);
}
} // namespace

extern "C"
{
    MODULE_API auto serialWrite(int64_t handle, const void *buffer, int buffer_size, int timeout_ms, int /*multiplier*/,
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

        const int written = writeSome(h, buffer, buffer_size, timeout_ms);
        if (written < 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kWriteError);
        }

        FlushFileBuffers(h);

        return written;
    }

} // extern "C"
