#include <cpp_core/interface/serial_clear_buffer_in.h>

#include "detail/handle_state.hpp"
#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialClearBufferIn(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        cpp_bindings_windows::detail::HandleContext context;
        const auto status = cpp_bindings_windows::detail::acquireHandleContext<int>(handle, error_callback, &context);
        if (status < 0)
        {
            return status;
        }

        if (PurgeComm(context.handle, PURGE_RXCLEAR) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kClearBufferInError));
        }
        return static_cast<int>(cpp_core::StatusCode::kSuccess);
    }

} // extern "C"
