#include <cpp_core/interface/serial_get_flow_control.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialGetFlowControl(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        if (dcb.fOutxCtsFlow != 0 && dcb.fRtsControl == RTS_CONTROL_HANDSHAKE)
        {
            return 1;
        }
        if (dcb.fOutX != 0 && dcb.fInX != 0)
        {
            return 2;
        }
        return 0;
    }

} // extern "C"
