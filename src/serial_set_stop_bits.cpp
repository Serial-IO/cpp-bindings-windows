#include <cpp_core/interface/serial_set_stop_bits.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSetStopBits(int64_t handle, int stop_bits, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        if (stop_bits != 0 && stop_bits != 1 && stop_bits != 2)
        {
            return cpp_core::failMsg<int>(cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                                          cpp_core::StatusCode::Configuration::kSetStopBitsError,
                                          "Invalid stop bits: must be 0, 1, or 2");
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kGetStateError);
        }

        serial_settings.StopBits = (stop_bits == 2) ? TWOSTOPBITS : ONESTOPBIT;

        if (SetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Configuration::kSetStopBitsError);
        }

        return 0;
    }

} // extern "C"
