#include <cpp_core/interface/serial_write.h>

#include "detail/write_impl.hpp"

extern "C"
{

    MODULE_API auto serialWrite(int64_t handle, const std::uint8_t *buffer, int buffer_size,
                                const cpp_core::SerialTimeoutConfig *timeout_config, ErrorCallbackT error_callback)
        -> int
    {
        return cpp_bindings_windows::detail::writeImpl(handle, buffer, buffer_size, timeout_config, error_callback);
    }

} // extern "C"
