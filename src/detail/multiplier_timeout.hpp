#pragma once

#include <cpp_core/validation.hpp>

#include <climits>
#include <cstdint>

namespace cpp_bindings_windows::detail
{
inline auto multiplierTimeout(int timeout_ms, int multiplier) -> int
{
    if (multiplier <= 0)
    {
        return 0;
    }

    const auto timeout = static_cast<int64_t>(cpp_core::clampTimeout(timeout_ms)) * multiplier;
    return timeout > INT_MAX ? INT_MAX : static_cast<int>(timeout);
}
} // namespace cpp_bindings_windows::detail
