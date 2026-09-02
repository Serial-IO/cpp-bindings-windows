#pragma once

#include "acquire_handle_context.hpp"
#include "bytes_waiting.hpp"
#include "consume_abort.hpp"
#include "copy_until_terminator.hpp"
#include "fail_win32.hpp"
#include "multiplier_timeout.hpp"
#include "note_bytes_transferred.hpp"
#include "read_chunk.hpp"

#include <cpp_core/validation.hpp>

#include <algorithm>
#include <array>

namespace cpp_bindings_windows::detail
{
inline constexpr int kTerminatedReadChunkSize = 4096;

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
    if (consumeAbort(context.state, Operation::kRead))
    {
        return cpp_core::failMsg<int>(callback, static_cast<StatusCodeValue>(StatusCode::Io::kAbortReadError),
                                      "Read aborted");
    }

    auto *output = static_cast<unsigned char *>(buffer);
    const auto buffered = context.state->read_ahead.consume(output, 0, buffer_size, terminator, terminator_size);
    int total_read = buffered.bytes_copied;
    if (total_read > 0)
    {
        noteBytesTransferred(context.state, Operation::kRead, total_read);
    }
    if (buffered.terminator_found)
    {
        return total_read;
    }

    while (total_read < buffer_size)
    {
        int waiting = 0;
        if (!bytesWaiting(context.handle, &waiting))
        {
            return failWin32<int>(callback, static_cast<StatusCodeValue>(StatusCode::Control::kGetStateError));
        }

        int chunk_size = waiting > 0 ? std::min(waiting, buffer_size - total_read) : 1;
        if (terminator_size > 0)
        {
            chunk_size = std::min(chunk_size, kTerminatedReadChunkSize);
        }

        const int current_timeout =
            total_read == 0 ? cpp_core::clampTimeout(timeout_ms) : multiplierTimeout(timeout_ms, multiplier);
        std::array<unsigned char, kTerminatedReadChunkSize> chunk{};
        unsigned char *destination = terminator_size > 0 ? chunk.data() : output + total_read;
        const auto result = readChunk(context, destination, chunk_size, current_timeout);
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

        if (terminator_size <= 0)
        {
            noteBytesTransferred(context.state, Operation::kRead, result.bytes_transferred);
            total_read += result.bytes_transferred;
            continue;
        }

        const auto copied = copyUntilTerminator(output, total_read, chunk.data(), result.bytes_transferred, terminator,
                                                terminator_size);
        if (copied.bytes_copied < result.bytes_transferred)
        {
            context.state->read_ahead.append(chunk.data() + copied.bytes_copied,
                                             result.bytes_transferred - copied.bytes_copied);
        }

        noteBytesTransferred(context.state, Operation::kRead, copied.bytes_copied);
        total_read += copied.bytes_copied;
        if (copied.terminator_found)
        {
            return total_read;
        }
    }

    return total_read;
}
} // namespace cpp_bindings_windows::detail
