#include <cpp_core/interface/serial_set_baudrate.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSetBaudrate(int64_t handle, int baudrate, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        if (baudrate < 300)
        {
            return cpp_core::failMsg<int>(error_callback, cpp_core::StatusCodes::kSetBaudrateError,
                                          "Invalid baudrate: must be >= 300");
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        dcb.BaudRate = static_cast<DWORD>(baudrate);

        if (SetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSetBaudrateError);
        }

        return 0;
    }

} // extern "C"
