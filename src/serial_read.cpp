#include <cpp_core/interface/serial_read.h>

#include "detail/read_impl.hpp"

extern "C"
{

    MODULE_API auto serialRead(int64_t handle, void *buffer, int buffer_size, int timeout_ms, int multiplier,
                               ErrorCallbackT error_callback) -> int
    {
        return cpp_bindings_windows::detail::readImpl(handle, buffer, buffer_size, timeout_ms, multiplier, nullptr, 0,
                                                      error_callback);
    }

} // extern "C"
