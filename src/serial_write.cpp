#include <cpp_core/interface/serial_write.h>
#include <cpp_core/status_codes.h>

#include "detail/win32_helpers.hpp"

#include <limits>

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

    DWORD bytes_written = 0;
    const BOOL ok = WriteFile(handle, src, static_cast<DWORD>(size), &bytes_written, &ov);
    if (ok != 0)
    {
        CloseHandle(ov.hEvent);
        return static_cast<int>(bytes_written);
    }

    const DWORD err = GetLastError();
    if (err != ERROR_IO_PENDING)
    {
        CloseHandle(ov.hEvent);
        SetLastError(err);
        return -1;
    }

    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }
    const DWORD wait_rc = WaitForSingleObject(ov.hEvent, static_cast<DWORD>(timeout_ms));
    if (wait_rc == WAIT_TIMEOUT)
    {
        CancelIoEx(handle, &ov);
        CloseHandle(ov.hEvent);
        return 0;
    }
    if (wait_rc != WAIT_OBJECT_0)
    {
        CancelIoEx(handle, &ov);
        CloseHandle(ov.hEvent);
        SetLastError(ERROR_GEN_FAILURE);
        return -1;
    }

    if (GetOverlappedResult(handle, &ov, &bytes_written, FALSE) == 0)
    {
        const DWORD err2 = GetLastError();
        CloseHandle(ov.hEvent);
        SetLastError(err2);
        return -1;
    }

    CloseHandle(ov.hEvent);
    return static_cast<int>(bytes_written);
}
} // namespace

extern "C"
{
    MODULE_API auto serialWrite(int64_t handle, const void *buffer, int buffer_size, int timeout_ms, int /*multiplier*/,
                                ErrorCallbackT error_callback) -> int
    {
        if (buffer == nullptr || buffer_size <= 0)
        {
            return cpp_bindings_windows::detail::failMsg<int>(error_callback, cpp_core::StatusCodes::kBufferError,
                                                              "Invalid buffer or buffer_size");
        }

        if (handle <= 0 || handle > std::numeric_limits<int>::max() ||
            handle > std::numeric_limits<intptr_t>::max())
        {
            return cpp_bindings_windows::detail::failMsg<int>(
                error_callback, cpp_core::StatusCodes::kInvalidHandleError, "Invalid handle");
        }

        const HANDLE h = reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
        if (h == nullptr || h == INVALID_HANDLE_VALUE)
        {
            return cpp_bindings_windows::detail::failMsg<int>(
                error_callback, cpp_core::StatusCodes::kInvalidHandleError, "Invalid handle");
        }

        const int written = writeSome(h, buffer, buffer_size, timeout_ms);
        if (written < 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kWriteError);
        }

        // Best-effort drain (similar to tcdrain). This may block; callers can keep
        // timeouts small.
        FlushFileBuffers(h);

        return written;
    }

} // extern "C"
