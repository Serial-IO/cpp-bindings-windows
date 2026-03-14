#include <cpp_core/interface/serial_send_break.h>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

extern "C"
{

    MODULE_API auto serialSendBreak(int64_t handle, int duration_ms, ErrorCallbackT error_callback) -> int
    {
        HANDLE h = nullptr;
        const auto rc = cpp_bindings_windows::detail::validateWin32Handle<int>(handle, error_callback, &h);
        if (rc < 0)
        {
            return rc;
        }

        if (duration_ms <= 0)
        {
            return cpp_core::failMsg<int>(error_callback, cpp_core::StatusCodes::kSendBreakError,
                                          "Break duration must be > 0");
        }

        if (SetCommBreak(h) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSendBreakError);
        }

        Sleep(static_cast<DWORD>(duration_ms));

        if (ClearCommBreak(h) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kSendBreakError);
        }

        return 0;
    }

} // extern "C"
