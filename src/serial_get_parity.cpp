#include <cpp_core/interface/serial_get_parity.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialGetParity(int64_t handle, ErrorCallbackT error_callback) -> cpp_core::Parity
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return static_cast<cpp_core::Parity>(status);
        }

        DCB serial_settings = {};
        serial_settings.DCBlength = sizeof(DCB);
        if (GetCommState(native_handle, &serial_settings) == 0)
        {
            return static_cast<cpp_core::Parity>(cpp_bindings_windows::detail::failWin32<int>(
                error_callback, cpp_core::StatusCode::Control::kGetStateError));
        }

        switch (serial_settings.Parity)
        {
        case EVENPARITY:
            return static_cast<cpp_core::Parity>(1);
        case ODDPARITY:
            return static_cast<cpp_core::Parity>(2);
        default:
            return static_cast<cpp_core::Parity>(0);
        }
    }

} // extern "C"
