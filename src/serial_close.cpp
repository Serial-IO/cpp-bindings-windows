#include <cpp_core/interface/serial_close.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/remove_handle_state.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialClose(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        if (handle <= 0)
        {
            return 0;
        }

        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        if (CloseHandle(native_handle) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                cpp_core::StatusCode::Connection::kCloseHandleError);
        }

        cpp_bindings_windows::detail::removeHandleState(native_handle);
        return static_cast<int>(cpp_core::StatusCode::kSuccess);
    }

} // extern "C"
