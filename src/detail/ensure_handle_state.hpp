#pragma once

#include "handle_key.hpp"

namespace cpp_bindings_windows::detail
{
inline auto ensureHandleState(HANDLE handle) -> std::shared_ptr<HandleState>
{
    std::lock_guard lock(g_handle_states_mutex);
    auto &state = g_handle_states[handleKey(handle)];
    if (!state)
    {
        state = std::make_shared<HandleState>();
    }
    return state;
}
} // namespace cpp_bindings_windows::detail
