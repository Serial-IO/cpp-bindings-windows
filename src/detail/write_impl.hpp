#pragma once

#include "acquire_handle_context.hpp"
#include "fail_win32.hpp"
#include "multiplier_timeout.hpp"
#include "note_bytes_transferred.hpp"
#include "write_chunk.hpp"

#include <cpp_core/validation.hpp>

namespace cpp_bindings_windows::detail
{
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
