#pragma once

#include "finish_pending_io.hpp"
#include "io_types.hpp"

#include <cpp_core/validation.hpp>

namespace cpp_bindings_windows::detail
{
inline auto waitForPendingIo(HANDLE handle, const std::shared_ptr<HandleState> &state, Operation operation,
                             OVERLAPPED *overlapped, int timeout_ms) -> IoResult
{
    const DWORD wait_result =
        WaitForSingleObject(overlapped->hEvent, static_cast<DWORD>(cpp_core::clampTimeout(timeout_ms)));
    if (wait_result == WAIT_TIMEOUT)
    {
        (void)CancelIoEx(handle, overlapped);
        DWORD transferred = 0;
        const BOOL completed = GetOverlappedResult(handle, overlapped, &transferred, TRUE);
        const DWORD error = completed != FALSE ? ERROR_SUCCESS : GetLastError();
        if (finishPendingIo(state, operation, overlapped))
        {
            return {.outcome = IoOutcome::kAborted};
        }
        if (completed != FALSE || transferred > 0)
        {
            return {.outcome = IoOutcome::kCompleted, .bytes_transferred = static_cast<int>(transferred)};
        }
        if (error == ERROR_OPERATION_ABORTED)
        {
            return {.outcome = IoOutcome::kTimedOut};
        }
        return {.outcome = IoOutcome::kError, .error = error};
    }

    if (wait_result != WAIT_OBJECT_0)
    {
        const DWORD error = GetLastError();
        (void)CancelIoEx(handle, overlapped);
        DWORD ignored = 0;
        (void)GetOverlappedResult(handle, overlapped, &ignored, TRUE);
        const bool aborted = finishPendingIo(state, operation, overlapped);
        return {.outcome = aborted ? IoOutcome::kAborted : IoOutcome::kError, .error = error};
    }

    DWORD transferred = 0;
    const BOOL completed = GetOverlappedResult(handle, overlapped, &transferred, FALSE);
    const DWORD error = completed != FALSE ? ERROR_SUCCESS : GetLastError();
    const bool aborted = finishPendingIo(state, operation, overlapped);
    if (aborted || error == ERROR_OPERATION_ABORTED)
    {
        return {.outcome = IoOutcome::kAborted};
    }
    if (completed == FALSE)
    {
        return {.outcome = IoOutcome::kError, .error = error};
    }
    return {.outcome = IoOutcome::kCompleted, .bytes_transferred = static_cast<int>(transferred)};
}
} // namespace cpp_bindings_windows::detail
