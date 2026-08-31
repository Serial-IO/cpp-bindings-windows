#include <cpp_core/interface/serial_send_break.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSendBreak(int64_t handle, int duration_ms, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        if (duration_ms <= 0)
        {
            return cpp_core::failMsg<int>(cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                                          cpp_core::StatusCode::Control::kSendBreakError, "Break duration must be > 0");
        }

        if (SetCommBreak(native_handle) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kSendBreakError);
        }

        Sleep(static_cast<DWORD>(duration_ms));

        if (ClearCommBreak(native_handle) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kSendBreakError);
        }

        return 0;
    }

} // extern "C"
