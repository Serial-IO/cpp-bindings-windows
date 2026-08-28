#include <cpp_core/interface/serial_out_bytes_waiting.h>

#include "detail/acquire_handle_context.hpp"
#include "detail/fail_win32.hpp"

#include <climits>

extern "C"
{

    MODULE_API auto serialOutBytesWaiting(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        cpp_bindings_windows::detail::HandleContext context;
        const auto status = cpp_bindings_windows::detail::acquireHandleContext<int>(handle, error_callback, &context);
        if (status < 0)
        {
            return status;
        }

        DWORD errors = 0;
        COMSTAT comm_status = {};
        if (ClearCommError(context.handle, &errors, &comm_status) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Control::kGetStateError));
        }
        return comm_status.cbOutQue > static_cast<DWORD>(INT_MAX) ? INT_MAX : static_cast<int>(comm_status.cbOutQue);
    }

} // extern "C"
