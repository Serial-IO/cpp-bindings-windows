#include <cpp_core/interface/serial_set_parity.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSetParity(int64_t handle, int parity, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        BYTE windows_parity = NOPARITY;
        switch (parity)
        {
        case 0:
            windows_parity = NOPARITY;
            break;
        case 1:
            windows_parity = EVENPARITY;
            break;
        case 2:
            windows_parity = ODDPARITY;
            break;
        default:
            return cpp_core::failMsg<int>(cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                                          cpp_core::StatusCode::Configuration::kSetParityError,
                                          "Invalid parity: must be 0, 1, or 2");
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kGetStateError);
        }

        serial_settings.Parity = windows_parity;
        serial_settings.fParity = (parity != 0) ? TRUE : FALSE;

        if (SetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Configuration::kSetParityError);
        }

        return 0;
    }

} // extern "C"
