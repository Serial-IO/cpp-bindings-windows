#pragma once

#include "handle_types.hpp"

namespace cpp_bindings_windows::detail
{
inline auto abortFlag(const std::shared_ptr<HandleState> &state, Operation operation) -> std::atomic<bool> &
{
    return operation == Operation::kRead ? state->abort_read : state->abort_write;
}
} // namespace cpp_bindings_windows::detail
