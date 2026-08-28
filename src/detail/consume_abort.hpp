#pragma once

#include "abort_flag.hpp"

namespace cpp_bindings_windows::detail
{
inline auto consumeAbort(const std::shared_ptr<HandleState> &state, Operation operation) -> bool
{
    return abortFlag(state, operation).exchange(false, std::memory_order_acq_rel);
}
} // namespace cpp_bindings_windows::detail
