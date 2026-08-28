#pragma once

#include "ensure_handle_state.hpp"
#include "validate_win32_handle.hpp"

namespace cpp_bindings_windows::detail
{
template <cpp_core::StatusConvertible ReturnType>
inline auto acquireHandleContext(int64_t handle, ErrorCallbackT error_callback, HandleContext *out_context)
    -> ReturnType
{
    HANDLE native_handle = nullptr;
    const auto status = validateWin32Handle<ReturnType>(handle, error_callback, &native_handle);
    if (status < 0)
    {
        return status;
    }

    out_context->handle = native_handle;
    out_context->state = ensureHandleState(native_handle);
    return static_cast<ReturnType>(StatusCode::kSuccess);
}
} // namespace cpp_bindings_windows::detail
