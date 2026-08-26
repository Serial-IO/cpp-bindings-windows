#include <cpp_core/interface/serial_out_bytes_total.h>

#include "detail/handle_state.hpp"

extern "C"
{

    MODULE_API auto serialOutBytesTotal(int64_t handle, ErrorCallbackT error_callback) -> int64_t
    {
        cpp_bindings_windows::detail::HandleContext context;
        const auto status =
            cpp_bindings_windows::detail::acquireHandleContext<int64_t>(handle, error_callback, &context);
        if (status < 0)
        {
            return status;
        }
        return context.state->bytes_written_total.load(std::memory_order_relaxed);
    }

} // extern "C"
