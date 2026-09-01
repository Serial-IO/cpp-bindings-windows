#pragma once

#include "abort_flag.hpp"
#include "pending_operation.hpp"

namespace cpp_bindings_windows::detail
{
inline auto requestAbort(HANDLE handle, const std::shared_ptr<HandleState> &state, Operation operation) -> void
{
    abortFlag(state, operation).store(true, std::memory_order_release);

    std::lock_guard lock(state->pending_io_mutex);
    if (auto *pending = pendingOperation(state, operation); pending != nullptr)
    {
        (void)CancelIoEx(handle, pending);
    }
}
} // namespace cpp_bindings_windows::detail
