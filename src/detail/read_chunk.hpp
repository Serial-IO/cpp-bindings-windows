#pragma once

#include "start_pending_io.hpp"
#include "wait_for_pending_io.hpp"

namespace cpp_bindings_windows::detail
{
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
} // namespace cpp_bindings_windows::detail
