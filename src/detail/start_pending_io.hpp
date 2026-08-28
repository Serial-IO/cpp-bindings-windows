#pragma once

#include "consume_abort.hpp"
#include "pending_operation.hpp"

#include <utility>

namespace cpp_bindings_windows::detail
{
template <typename StartOperation>
inline auto startPendingIo(const std::shared_ptr<HandleState> &state, Operation operation, OVERLAPPED *overlapped,
                           StartOperation &&start_operation) -> PendingIoStart
{
    std::lock_guard lock(state->pending_io_mutex);
    if (consumeAbort(state, operation))
    {
        return {.aborted = true};
    }

    pendingOperation(state, operation) = overlapped;
    const BOOL completed = std::forward<StartOperation>(start_operation)();
    return {.completed = completed, .error = completed != FALSE ? ERROR_SUCCESS : GetLastError()};
}
} // namespace cpp_bindings_windows::detail
