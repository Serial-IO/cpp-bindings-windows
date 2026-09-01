#include <cpp_core/interface/serial_set_flow_control.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSetFlowControl(int64_t handle, int mode, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        if (mode < 0 || mode > 2)
        {
            return cpp_core::failMsg<int>(cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                                          cpp_core::StatusCode::Configuration::kSetFlowControlError,
                                          "Invalid flow control mode: must be 0, 1, or 2");
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kGetStateError);
        }

        serial_settings.fOutxCtsFlow = FALSE;
        serial_settings.fRtsControl = RTS_CONTROL_ENABLE;
        serial_settings.fOutX = FALSE;
        serial_settings.fInX = FALSE;

        switch (mode)
        {
        case 1:
            serial_settings.fOutxCtsFlow = TRUE;
            serial_settings.fRtsControl = RTS_CONTROL_HANDSHAKE;
            break;
        case 2:
            serial_settings.fOutX = TRUE;
            serial_settings.fInX = TRUE;
            serial_settings.XonChar = 0x11;
            serial_settings.XoffChar = 0x13;
            serial_settings.XonLim = 2048;
            serial_settings.XoffLim = 512;
            break;
        default:
            break;
        }

        if (SetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                error_callback, cpp_core::StatusCode::Configuration::kSetFlowControlError);
        }

        return 0;
    }

} // extern "C"
