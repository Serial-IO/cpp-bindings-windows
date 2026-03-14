#include <cpp_core/interface/serial_set_stop_bits.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSetStopBits(int64_t handle, int stop_bits, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        if (stop_bits != 0 && stop_bits != 1 && stop_bits != 2)
        {
            return cpp_core::failMsg<int>(error_callback, cpp_core::StatusCodes::kSetStopBitsError,
                                          "Invalid stop bits: must be 0, 1, or 2");
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        dcb.StopBits = (stop_bits == 2) ? TWOSTOPBITS : ONESTOPBIT;

        if (SetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSetStopBitsError);
        }

        return 0;
    }

} // extern "C"
