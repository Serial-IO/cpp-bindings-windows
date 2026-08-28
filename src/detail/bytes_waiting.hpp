#pragma once

#include "windows.hpp"

#include <climits>

namespace cpp_bindings_windows::detail
{
inline auto bytesWaiting(HANDLE handle, int *out_bytes) -> bool
{
    if (out_bytes == nullptr)
    {
        return false;
    }
    *out_bytes = 0;

    DWORD errors = 0;
    COMSTAT status = {};
    if (ClearCommError(handle, &errors, &status) == 0)
    {
        return false;
    }

    *out_bytes = status.cbInQue > static_cast<DWORD>(INT_MAX) ? INT_MAX : static_cast<int>(status.cbInQue);
    return true;
}
} // namespace cpp_bindings_windows::detail
