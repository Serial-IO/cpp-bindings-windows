#include <cpp_core/interface/serial_set_flow_control.h>
#include <cpp_core/validation.hpp>

#include "detail/apply_flow_control.hpp"
#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSetFlowControl(int64_t handle, cpp_core::FlowControl mode, ErrorCallbackT error_callback)
        -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        if (cpp_core::toInt(mode) < 0 || cpp_core::toInt(mode) > 2)
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

        cpp_bindings_windows::detail::applyFlowControl(serial_settings, mode);

        if (SetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                error_callback, cpp_core::StatusCode::Configuration::kSetFlowControlError);
        }

        return 0;
    }

} // extern "C"
