#include <cpp_core/interface/serial_set_dtr.h>
#include <cpp_core/validation.hpp>

#include "detail/fail_win32.hpp"
#include "detail/validate_win32_handle.hpp"

extern "C"
{

    MODULE_API auto serialSetDtr(int64_t handle, int state, ErrorCallbackT error_callback) -> int
    {
        HANDLE native_handle = nullptr;
        const auto status =
            cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &native_handle);
        if (status < 0)
        {
            return status;
        }

        const DWORD communication_function = state ? SETDTR : CLRDTR;
        if (EscapeCommFunction(native_handle, communication_function) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCode::Control::kSetDtrError);
        }

        return 0;
    }

} // extern "C"
