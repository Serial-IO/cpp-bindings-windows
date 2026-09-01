#include <cpp_core/interface/serial_read_line.h>

#include "detail/read_impl.hpp"

extern "C"
{

    MODULE_API auto serialReadLine(int64_t handle, void *buffer, int buffer_size, int timeout_ms, int multiplier,
                                   ErrorCallbackT error_callback) -> int
    {
        static constexpr unsigned char kNewline = '\n';
        return cpp_bindings_windows::detail::readImpl(handle, buffer, buffer_size, timeout_ms, multiplier, &kNewline, 1,
                                                      error_callback);
    }

} // extern "C"
