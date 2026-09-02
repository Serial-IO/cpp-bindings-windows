#include <cpp_core/interface/serial_in_bytes_waiting.h>

#include "detail/acquire_handle_context.hpp"
#include "detail/bytes_waiting.hpp"
#include "detail/fail_win32.hpp"

#include <climits>
#include <cstdint>

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
        const auto total_waiting = static_cast<int64_t>(waiting) + context.state->read_ahead.size();
        return total_waiting > INT_MAX ? INT_MAX : static_cast<int>(total_waiting);
    }

} // extern "C"
