#pragma once

#include "multiplier_timeout.hpp"

#include <cpp_core/validation.hpp>

namespace cpp_bindings_windows::detail
{
inline auto readTimeout(int timeout_ms, int multiplier, bool first_read, bool terminated_read) -> int
{
    // A terminated read must keep waiting for the terminator even when callers
    // use the raw-read default multiplier of zero. Otherwise it only drains the
    // bytes that happened to be queued when the call started.
    if (first_read || (terminated_read && multiplier <= 0))
    {
        return cpp_core::clampTimeout(timeout_ms);
    }

    return multiplierTimeout(timeout_ms, multiplier);
}
} // namespace cpp_bindings_windows::detail
