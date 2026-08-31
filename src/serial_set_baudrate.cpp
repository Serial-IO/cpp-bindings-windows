#include <cpp_core/interface/serial_set_baudrate.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSetBaudrate(int64_t handle, int baudrate, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        if (baudrate < 300)
        {
            return cpp_core::failMsg<int>(cpp_bindings_windows::detail::effectiveErrorCallback(error_callback),
                                          cpp_core::StatusCode::Configuration::kSetBaudrateError,
                                          "Invalid baudrate: must be >= 300");
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kGetStateError);
        }

        serial_settings.BaudRate = static_cast<DWORD>(baudrate);

        if (SetCommState(native_handle, &serial_settings) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Configuration::kSetBaudrateError);
        }

        return 0;
    }

} // extern "C"
