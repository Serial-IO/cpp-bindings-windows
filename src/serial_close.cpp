#include <cpp_core/interface/serial_close.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialClose(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        if (handle <= 0)
        {
            return 0;
        }

        HANDLE h = nullptr;
        const auto handle_ok =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (handle_ok < 0)
        {
            return handle_ok;
        }

        if (CloseHandle(h) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kCloseHandleError);
        }

        return 0;
    }

} // extern "C"
