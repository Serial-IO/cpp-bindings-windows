#pragma once

#include "consume_abort.hpp"
#include "pending_operation.hpp"

namespace cpp_bindings_windows::detail
{
inline auto finishPendingIo(const std::shared_ptr<HandleState> &state, Operation operation, OVERLAPPED *overlapped)
    -> bool
{
    std::lock_guard lock(state->pending_io_mutex);
    auto &pending = pendingOperation(state, operation);
    if (pending == overlapped)
    {
        pending = nullptr;
    }
    return consumeAbort(state, operation);
}
} // namespace cpp_bindings_windows::detail
