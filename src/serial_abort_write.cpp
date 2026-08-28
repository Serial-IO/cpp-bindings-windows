#include <cpp_core/interface/serial_abort_write.h>

#include "detail/acquire_handle_context.hpp"
#include "detail/request_abort.hpp"

extern "C"
{

    MODULE_API auto serialAbortWrite(int64_t handle, ErrorCallbackT error_callback) -> int
    {
        cpp_bindings_windows::detail::HandleContext context;
        const auto status = cpp_bindings_windows::detail::acquireHandleContext<int>(handle, error_callback, &context);
        if (status < 0)
        {
            return status;
        }

        cpp_bindings_windows::detail::requestAbort(context.handle, context.state,
                                                   cpp_bindings_windows::detail::Operation::kWrite);
        return static_cast<int>(cpp_core::StatusCode::kSuccess);
    }

} // extern "C"
