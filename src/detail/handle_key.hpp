#pragma once

#include "handle_types.hpp"

namespace cpp_bindings_windows::detail
{
inline auto handleKey(HANDLE handle) -> std::uintptr_t
{
    return reinterpret_cast<std::uintptr_t>(handle);
}
} // namespace cpp_bindings_windows::detail
