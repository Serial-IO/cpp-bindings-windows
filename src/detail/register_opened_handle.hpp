#pragma once

#include "ensure_handle_state.hpp"

namespace cpp_bindings_windows::detail
{
inline auto registerOpenedHandle(HANDLE handle) -> void
{
    (void)ensureHandleState(handle);
}
} // namespace cpp_bindings_windows::detail
