#include <cpp_core/interface/serial_get_flow_control.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialGetFlowControl(int64_t handle, ErrorCallbackT error_callback) -> cpp_core::FlowControl
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return static_cast<cpp_core::FlowControl>(status);
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return static_cast<cpp_core::FlowControl>(cpp_bindings_windows::detail::failWin32<int>(
                error_callback, cpp_core::StatusCode::Control::kGetStateError));
        }

        if (serial_settings.fOutxCtsFlow != 0 && serial_settings.fRtsControl == RTS_CONTROL_HANDSHAKE)
        {
            return static_cast<cpp_core::FlowControl>(1);
        }
        if (serial_settings.fOutX != 0 && serial_settings.fInX != 0)
        {
            return static_cast<cpp_core::FlowControl>(2);
        }
        return static_cast<cpp_core::FlowControl>(0);
    }

} // extern "C"
