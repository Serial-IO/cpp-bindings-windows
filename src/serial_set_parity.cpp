#include <cpp_core/interface/serial_set_parity.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSetParity(int64_t handle, int parity, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        BYTE win_parity = NOPARITY;
        switch (parity)
        {
        case 0:
            win_parity = NOPARITY;
            break;
        case 1:
            win_parity = EVENPARITY;
            break;
        case 2:
            win_parity = ODDPARITY;
            break;
        default:
            return cpp_core::failMsg<int>(error_callback, cpp_core::StatusCodes::kSetParityError,
                                          "Invalid parity: must be 0, 1, or 2");
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        dcb.Parity = win_parity;
        dcb.fParity = (parity != 0) ? TRUE : FALSE;

        if (SetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSetParityError);
        }

        return 0;
    }

} // extern "C"
