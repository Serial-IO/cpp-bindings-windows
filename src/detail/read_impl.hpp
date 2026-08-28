#pragma once

#include "acquire_handle_context.hpp"
#include "bytes_waiting.hpp"
#include "fail_win32.hpp"
#include "matches_suffix.hpp"
#include "multiplier_timeout.hpp"
#include "note_bytes_transferred.hpp"
#include "read_chunk.hpp"

#include <cpp_core/validation.hpp>

#include <algorithm>

namespace cpp_bindings_windows::detail
{
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
} // namespace cpp_bindings_windows::detail
