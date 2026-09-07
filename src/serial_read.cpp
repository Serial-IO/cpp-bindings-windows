#include <cpp_core/interface/serial_read.h>

#include "detail/read_impl.hpp"

extern "C"
{

    MODULE_API auto serialRead(int64_t handle, std::uint8_t *buffer, int buffer_size,
                               const cpp_core::SerialTimeoutConfig *timeout_config, ErrorCallbackT error_callback)
        -> int
    {
        return cpp_bindings_windows::detail::readImpl(handle, buffer, buffer_size, timeout_config, nullptr, 0,
                                                      error_callback);
    }

} // extern "C"
