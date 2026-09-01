#include <cpp_core/interface/serial_in_bytes_waiting.h>

#include "detail/acquire_handle_context.hpp"
#include "detail/bytes_waiting.hpp"
#include "detail/fail_win32.hpp"

extern "C"
{

    MODULE_API auto serialInBytesWaiting(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        cpp_bindings_windows::detail::HandleContext context;
        const auto status = cpp_bindings_windows::detail::acquireHandleContext<int>(handle, error_callback, &context);
        if (status < 0)
        {
            return status;
        }

        int waiting = 0;
        if (!cpp_bindings_windows::detail::bytesWaiting(context.handle, &waiting))
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Control::kGetStateError));
        }
        return waiting;
    }

} // extern "C"
