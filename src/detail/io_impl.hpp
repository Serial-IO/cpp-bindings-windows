#pragma once

#include "handle_state.hpp"
#include "win32_helpers.hpp"

#include <cpp_core/validation.hpp>

#include <algorithm>
#include <climits>
#include <cstring>

namespace cpp_bindings_windows::detail
{
enum class IoOutcome
{
    kCompleted,
    kTimedOut,
    kAborted,
    kError,
};

struct IoResult
{
    IoOutcome outcome = IoOutcome::kError;
    int bytes_transferred = 0;
    DWORD error = ERROR_SUCCESS;
};

inline auto multiplierTimeout(int timeout_ms, int multiplier) -> int
{
    if (multiplier <= 0)
    {
        return 0;
    }

    const auto timeout = static_cast<int64_t>(cpp_core::clampTimeout(timeout_ms)) * multiplier;
    return timeout > INT_MAX ? INT_MAX : static_cast<int>(timeout);
}

inline auto waitForPendingIo(HANDLE handle, const std::shared_ptr<HandleState> &state, Operation operation,
                             OVERLAPPED *overlapped, int timeout_ms) -> IoResult
{
    const DWORD wait_result =
        WaitForSingleObject(overlapped->hEvent, static_cast<DWORD>(cpp_core::clampTimeout(timeout_ms)));
    if (wait_result == WAIT_TIMEOUT)
    {
        (void)CancelIoEx(handle, overlapped);
        DWORD ignored = 0;
        (void)GetOverlappedResult(handle, overlapped, &ignored, TRUE);
        if (finishPendingIo(state, operation, overlapped))
        {
            return {.outcome = IoOutcome::kAborted};
        }
        return {.outcome = IoOutcome::kTimedOut};
    }

    if (wait_result != WAIT_OBJECT_0)
    {
        const DWORD error = GetLastError();
        (void)CancelIoEx(handle, overlapped);
        DWORD ignored = 0;
        (void)GetOverlappedResult(handle, overlapped, &ignored, TRUE);
        const bool aborted = finishPendingIo(state, operation, overlapped);
        return {.outcome = aborted ? IoOutcome::kAborted : IoOutcome::kError, .error = error};
    }

    DWORD transferred = 0;
    const BOOL completed = GetOverlappedResult(handle, overlapped, &transferred, FALSE);
    const DWORD error = completed != FALSE ? ERROR_SUCCESS : GetLastError();
    const bool aborted = finishPendingIo(state, operation, overlapped);
    if (aborted || error == ERROR_OPERATION_ABORTED)
    {
        return {.outcome = IoOutcome::kAborted};
    }
    if (completed == FALSE)
    {
        return {.outcome = IoOutcome::kError, .error = error};
    }
    return {.outcome = IoOutcome::kCompleted, .bytes_transferred = static_cast<int>(transferred)};
}

inline auto readChunk(const HandleContext &context, unsigned char *buffer, int buffer_size, int timeout_ms) -> IoResult
{
    UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event)
    {
        return {.outcome = IoOutcome::kError, .error = GetLastError()};
    }

    OVERLAPPED overlapped = {};
    overlapped.hEvent = event.get();
    DWORD transferred = 0;
    const auto start = startPendingIo(context.state, Operation::kRead, &overlapped, [&] {
        return ReadFile(context.handle, buffer, static_cast<DWORD>(buffer_size), &transferred, &overlapped);
    });
    if (start.aborted)
    {
        return {.outcome = IoOutcome::kAborted};
    }
    if (start.completed != FALSE)
    {
        const bool aborted = finishPendingIo(context.state, Operation::kRead, &overlapped);
        return {.outcome = aborted ? IoOutcome::kAborted : IoOutcome::kCompleted,
                .bytes_transferred = static_cast<int>(transferred)};
    }
    if (start.error != ERROR_IO_PENDING)
    {
        const bool aborted = finishPendingIo(context.state, Operation::kRead, &overlapped);
        return {.outcome = aborted ? IoOutcome::kAborted : IoOutcome::kError, .error = start.error};
    }

    return waitForPendingIo(context.handle, context.state, Operation::kRead, &overlapped, timeout_ms);
}

inline auto writeChunk(const HandleContext &context, const unsigned char *buffer, int buffer_size, int timeout_ms)
    -> IoResult
{
    UniqueHandle event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
    if (!event)
    {
        return {.outcome = IoOutcome::kError, .error = GetLastError()};
    }

    OVERLAPPED overlapped = {};
    overlapped.hEvent = event.get();
    DWORD transferred = 0;
    const auto start = startPendingIo(context.state, Operation::kWrite, &overlapped, [&] {
        return WriteFile(context.handle, buffer, static_cast<DWORD>(buffer_size), &transferred, &overlapped);
    });
    if (start.aborted)
    {
        return {.outcome = IoOutcome::kAborted};
    }
    if (start.completed != FALSE)
    {
        const bool aborted = finishPendingIo(context.state, Operation::kWrite, &overlapped);
        return {.outcome = aborted ? IoOutcome::kAborted : IoOutcome::kCompleted,
                .bytes_transferred = static_cast<int>(transferred)};
    }
    if (start.error != ERROR_IO_PENDING)
    {
        const bool aborted = finishPendingIo(context.state, Operation::kWrite, &overlapped);
        return {.outcome = aborted ? IoOutcome::kAborted : IoOutcome::kError, .error = start.error};
    }

    return waitForPendingIo(context.handle, context.state, Operation::kWrite, &overlapped, timeout_ms);
}

inline auto matchesSuffix(const unsigned char *buffer, int buffer_size, const unsigned char *terminator,
                          int terminator_size) -> bool
{
    return terminator_size > 0 && buffer_size >= terminator_size &&
           std::memcmp(buffer + buffer_size - terminator_size, terminator, static_cast<std::size_t>(terminator_size)) ==
               0;
}

inline auto readImpl(int64_t handle, void *buffer, int buffer_size, int timeout_ms, int multiplier,
                     const unsigned char *terminator, int terminator_size, ErrorCallbackT error_callback) -> int
{
    const auto callback = effectiveErrorCallback(error_callback);
    const auto buffer_status = cpp_core::validateBuffer<int>(buffer, buffer_size, callback);
    if (buffer_status < 0)
    {
        return buffer_status;
    }
    if (terminator_size > 0 && terminator == nullptr)
    {
        return cpp_core::failMsg<int>(callback, static_cast<StatusCodeValue>(StatusCode::Io::kBufferError),
                                      "Invalid terminator");
    }

    HandleContext context;
    const auto handle_status = acquireHandleContext<int>(handle, callback, &context);
    if (handle_status < 0)
    {
        return handle_status;
    }

    auto *output = static_cast<unsigned char *>(buffer);
    int total_read = 0;
    while (total_read < buffer_size)
    {
        int chunk_size = 1;
        if (terminator_size <= 0)
        {
            int waiting = 0;
            if (!bytesWaiting(context.handle, &waiting))
            {
                return failWin32<int>(callback, static_cast<StatusCodeValue>(StatusCode::Control::kGetStateError));
            }
            chunk_size = waiting > 0 ? std::min(waiting, buffer_size - total_read) : 1;
        }

        const int current_timeout =
            total_read == 0 ? cpp_core::clampTimeout(timeout_ms) : multiplierTimeout(timeout_ms, multiplier);
        const auto result = readChunk(context, output + total_read, chunk_size, current_timeout);
        if (result.outcome == IoOutcome::kTimedOut)
        {
            return total_read;
        }
        if (result.outcome == IoOutcome::kAborted)
        {
            return cpp_core::failMsg<int>(callback, static_cast<StatusCodeValue>(StatusCode::Io::kAbortReadError),
                                          "Read aborted");
        }
        if (result.outcome == IoOutcome::kError)
        {
            SetLastError(result.error);
            return failWin32<int>(callback, static_cast<StatusCodeValue>(StatusCode::Io::kReadError));
        }
        if (result.bytes_transferred <= 0)
        {
            return total_read;
        }

        noteBytesTransferred(context.state, Operation::kRead, result.bytes_transferred);
        total_read += result.bytes_transferred;
        if (matchesSuffix(output, total_read, terminator, terminator_size))
        {
            return total_read;
        }
    }

    return total_read;
}

inline auto writeImpl(int64_t handle, const void *buffer, int buffer_size, int timeout_ms, int multiplier,
                      ErrorCallbackT error_callback) -> int
{
    const auto callback = effectiveErrorCallback(error_callback);
    const auto buffer_status = cpp_core::validateBuffer<int>(buffer, buffer_size, callback);
    if (buffer_status < 0)
    {
        return buffer_status;
    }

    HandleContext context;
    const auto handle_status = acquireHandleContext<int>(handle, callback, &context);
    if (handle_status < 0)
    {
        return handle_status;
    }

    const auto *input = static_cast<const unsigned char *>(buffer);
    int total_written = 0;
    while (total_written < buffer_size)
    {
        const int current_timeout =
            total_written == 0 ? cpp_core::clampTimeout(timeout_ms) : multiplierTimeout(timeout_ms, multiplier);
        const auto result = writeChunk(context, input + total_written, buffer_size - total_written, current_timeout);
        if (result.outcome == IoOutcome::kTimedOut)
        {
            return total_written;
        }
        if (result.outcome == IoOutcome::kAborted)
        {
            return cpp_core::failMsg<int>(callback, static_cast<StatusCodeValue>(StatusCode::Io::kAbortWriteError),
                                          "Write aborted");
        }
        if (result.outcome == IoOutcome::kError)
        {
            SetLastError(result.error);
            return failWin32<int>(callback, static_cast<StatusCodeValue>(StatusCode::Io::kWriteError));
        }
        if (result.bytes_transferred <= 0)
        {
            return total_written;
        }

        noteBytesTransferred(context.state, Operation::kWrite, result.bytes_transferred);
        total_written += result.bytes_transferred;
    }

    return total_written;
}

} // namespace cpp_bindings_windows::detail
