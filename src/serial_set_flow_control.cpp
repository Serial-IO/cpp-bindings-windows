#include <cpp_core/interface/serial_set_flow_control.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSetFlowControl(int64_t handle, int mode, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        if (mode < 0 || mode > 2)
        {
            return cpp_core::failMsg<int>(error_callback, cpp_core::StatusCodes::kSetFlowControlError,
                                          "Invalid flow control mode: must be 0, 1, or 2");
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        dcb.fOutxCtsFlow = FALSE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutX = FALSE;
        dcb.fInX = FALSE;

        switch (mode)
        {
        case 1:
            dcb.fOutxCtsFlow = TRUE;
            dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
            break;
        case 2:
            dcb.fOutX = TRUE;
            dcb.fInX = TRUE;
            dcb.XonChar = 0x11;
            dcb.XoffChar = 0x13;
            dcb.XonLim = 2048;
            dcb.XoffLim = 512;
            break;
        default:
            break;
        }

        if (SetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSetFlowControlError);
        }

        return 0;
    }

} // extern "C"
