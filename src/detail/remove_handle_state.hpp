#pragma once

#include "handle_key.hpp"

namespace cpp_bindings_windows::detail
{
inline auto removeHandleState(HANDLE handle) -> void
{
    std::lock_guard lock(g_handle_states_mutex);
    g_handle_states.erase(handleKey(handle));
}
} // namespace cpp_bindings_windows::detail
