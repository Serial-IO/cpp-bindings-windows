#include <cpp_core/interface/serial_get_dsr.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialGetDsr(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        DWORD modem_status = 0;
        if (GetCommModemStatus(h, &modem_status) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kGetModemStatusError);
        }

        return (modem_status & MS_DSR_ON) ? 1 : 0;
    }

} // extern "C"
