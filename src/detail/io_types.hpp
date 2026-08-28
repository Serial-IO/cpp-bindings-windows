#pragma once

#include "windows.hpp"

namespace cpp_bindings_windows::detail
{
enum class IoOutcome
{
    kCompleted,
    kTimedOut,
    kAborted,
    kError,
};

struct IoResult
{
    IoOutcome outcome = IoOutcome::kError;
    int bytes_transferred = 0;
    DWORD error = ERROR_SUCCESS;
};
} // namespace cpp_bindings_windows::detail
