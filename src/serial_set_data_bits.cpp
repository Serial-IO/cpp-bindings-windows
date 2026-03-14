#include <cpp_core/interface/serial_set_data_bits.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSetDataBits(int64_t handle, int data_bits, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        if (data_bits < 5 || data_bits > 8)
        {
            return cpp_core::failMsg<int>(error_callback, cpp_core::StatusCodes::kSetDataBitsError,
                                          "Invalid data bits: must be 5-8");
        }

        DCB dcb = {};
        dcb.DCBlength = sizeof(DCB);
        if (GetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kGetStateError);
        }

        dcb.ByteSize = static_cast<BYTE>(data_bits);

        if (SetCommState(h, &dcb) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSetDataBitsError);
        }

        return 0;
    }

} // extern "C"
