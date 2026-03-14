#include <cpp_core/interface/serial_set_dtr.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSetDtr(int64_t handle, int state, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        const DWORD func = state ? SETDTR : CLRDTR;
        if (EscapeCommFunction(h, func) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kSetDtrError);
        }

        return 0;
    }

} // extern "C"
