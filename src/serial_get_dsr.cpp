#include <cpp_core/interface/serial_get_dsr.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialGetDsr(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        DWORD modem_status = 0;
        if (GetCommModemStatus(native_handle, &modem_status) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kGetModemStatusError);
        }

        return (modem_status & MS_DSR_ON) ? 1 : 0;
    }

} // extern "C"
