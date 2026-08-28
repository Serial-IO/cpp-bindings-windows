#pragma once

#include "effective_error_callback.hpp"
#include "win32_error_to_string.hpp"

#include <cpp_core/error_handling.hpp>

namespace cpp_bindings_windows::detail
{
template <cpp_core::StatusConvertible ReturnType>
inline auto failWin32(ErrorCallbackT error_callback, StatusCodeValue code) -> ReturnType
{
    const DWORD error = GetLastError();
    const std::string message = win32ErrorToString(error);
    cpp_core::invokeError(effectiveErrorCallback(error_callback), code, message);
    return static_cast<ReturnType>(code);
}
} // namespace cpp_bindings_windows::detail
