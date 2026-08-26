#pragma once

#include <cpp_core/error_callback.h>
#include <cpp_core/status_code.h>

#include <atomic>

namespace cpp_bindings_windows::detail
{
using IoCallbackT = void (*)(int);
using StatusCodeValue = cpp_core::StatusCodeValue;
using cpp_core::StatusCode;

inline std::atomic<ErrorCallbackT> g_error_callback{nullptr};
} // namespace cpp_bindings_windows::detail
