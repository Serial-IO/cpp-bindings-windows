#include <cpp_core/interface/serial_clear_buffer_out.h>

#include "detail/handle_state.hpp"
#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialClearBufferOut(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        cpp_bindings_windows::detail::HandleContext context;
        const auto status = cpp_bindings_windows::detail::acquireHandleContext<int>(handle, error_callback, &context);
        if (status < 0)
        {
            return status;
        }

        if (FlushFileBuffers(context.handle) == 0 || PurgeComm(context.handle, PURGE_TXCLEAR) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kClearBufferOutError));
        }
        return static_cast<int>(cpp_core::StatusCode::kSuccess);
    }

} // extern "C"
