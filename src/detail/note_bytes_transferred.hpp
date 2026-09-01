#pragma once

#include "handle_types.hpp"

namespace cpp_bindings_windows::detail
{
inline auto noteBytesTransferred(const std::shared_ptr<HandleState> &state, Operation operation, int transferred_bytes)
    -> void
{
    if (operation == Operation::kRead)
    {
        state->bytes_read_total.fetch_add(transferred_bytes, std::memory_order_relaxed);
        if (const auto callback = g_read_callback.load(std::memory_order_acquire); callback != nullptr)
        {
            callback(transferred_bytes);
        }
        return;
    }

    state->bytes_written_total.fetch_add(transferred_bytes, std::memory_order_relaxed);
    if (const auto callback = g_write_callback.load(std::memory_order_acquire); callback != nullptr)
    {
        callback(transferred_bytes);
    }
}
} // namespace cpp_bindings_windows::detail
