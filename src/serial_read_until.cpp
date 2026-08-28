#include <cpp_core/interface/serial_read_until.h>

#include "detail/read_impl.hpp"

extern "C"
{

    MODULE_API auto serialReadUntil(int64_t handle, void *buffer, int buffer_size, int timeout_ms, int multiplier,
                                    void *until_char, ErrorCallbackT error_callback) -> int
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        if (until_char == nullptr)
        {
            return cpp_core::failMsg<int>(
                callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kBufferError),
                "Terminator pointer must not be null");
        }

        return cpp_bindings_windows::detail::readImpl(handle, buffer, buffer_size, timeout_ms, multiplier,
                                                      static_cast<const unsigned char *>(until_char), 1, callback);
    }

} // extern "C"
