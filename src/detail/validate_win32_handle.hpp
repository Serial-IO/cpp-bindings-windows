#pragma once

#include "effective_error_callback.hpp"
#include "handle_types.hpp"

#include <cpp_core/error_handling.hpp>

#include <limits>

namespace cpp_bindings_windows::detail
{
template <cpp_core::StatusConvertible ReturnType>
inline auto validateWin32Handle(int64_t handle, ErrorCallbackT error_callback, HANDLE *out_handle) -> ReturnType
{
    const auto callback = effectiveErrorCallback(error_callback);
    if (handle <= 0)
    {
        return cpp_core::failMsg<ReturnType>(
            callback, static_cast<StatusCodeValue>(StatusCode::Connection::kInvalidHandleError), "Invalid handle");
    }

    if constexpr (sizeof(intptr_t) < sizeof(int64_t))
    {
        if (handle > static_cast<int64_t>(std::numeric_limits<intptr_t>::max()))
        {
            return cpp_core::failMsg<ReturnType>(
                callback, static_cast<StatusCodeValue>(StatusCode::Connection::kInvalidHandleError), "Invalid handle");
        }
    }

    const auto native_handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
    if (native_handle == nullptr || native_handle == INVALID_HANDLE_VALUE)
    {
        return cpp_core::failMsg<ReturnType>(
            callback, static_cast<StatusCodeValue>(StatusCode::Connection::kInvalidHandleError), "Invalid handle");
    }

    *out_handle = native_handle;
    return static_cast<ReturnType>(StatusCode::kSuccess);
}
} // namespace cpp_bindings_windows::detail
