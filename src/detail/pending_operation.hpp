#pragma once

#include "handle_types.hpp"

namespace cpp_bindings_windows::detail
{
inline auto pendingOperation(const std::shared_ptr<HandleState> &state, Operation operation) -> OVERLAPPED *&
{
    return operation == Operation::kRead ? state->pending_read : state->pending_write;
}
} // namespace cpp_bindings_windows::detail
