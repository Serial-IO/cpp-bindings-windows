#include <cpp_core/interface/serial_write.h>

#include "detail/write_impl.hpp"

extern "C"
{

    MODULE_API auto serialWrite(int64_t handle, const void *buffer, int buffer_size, int timeout_ms, int multiplier,
                                ErrorCallbackT error_callback) -> int
    {
        return cpp_bindings_windows::detail::writeImpl(handle, buffer, buffer_size, timeout_ms, multiplier,
                                                       error_callback);
    }

} // extern "C"
