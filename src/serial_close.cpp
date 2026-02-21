#include <cpp_core/interface/serial_close.h>
#include <cpp_core/status_codes.h>

#include "detail/win32_helpers.hpp"

#include <limits>

extern "C"
{

    MODULE_API auto serialClose(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        if (handle <= 0)
        {
            return static_cast<int>(cpp_core::StatusCodes::kSuccess);
        }

        if (handle > std::numeric_limits<int>::max() ||
            handle > std::numeric_limits<intptr_t>::max())
        {
            return cpp_bindings_windows::detail::failMsg<int>(
                error_callback, cpp_core::StatusCodes::kInvalidHandleError, "Invalid handle");
        }

        const HANDLE h = reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
        if (h == nullptr || h == INVALID_HANDLE_VALUE)
        {
            return static_cast<int>(cpp_core::StatusCodes::kSuccess);
        }

        if (CloseHandle(h) == 0)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback,
                                                                cpp_core::StatusCodes::kCloseHandleError);
        }

        return static_cast<int>(cpp_core::StatusCodes::kSuccess);
    }

} // extern "C"
