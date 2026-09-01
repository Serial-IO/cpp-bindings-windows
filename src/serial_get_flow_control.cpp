#include <cpp_core/interface/serial_get_flow_control.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialGetFlowControl(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kGetStateError);
        }

        if (serial_settings.fOutxCtsFlow != 0 && serial_settings.fRtsControl == RTS_CONTROL_HANDSHAKE)
        {
            return 1;
        }
        if (serial_settings.fOutX != 0 && serial_settings.fInX != 0)
        {
            return 2;
        }
        return 0;
    }

} // extern "C"
