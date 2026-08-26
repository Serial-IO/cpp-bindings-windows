#include <cpp_core/interface/serial_read_until_sequence.h>

#include "detail/io_impl.hpp"

#include <cstring>

extern "C"
{

    MODULE_API auto serialReadUntilSequence(int64_t handle, void *buffer, int buffer_size, int timeout_ms,
                                            int multiplier, void *sequence, ErrorCallbackT error_callback) -> int
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        if (sequence == nullptr)
        {
            return cpp_core::failMsg<int>(
                callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kBufferError),
                "Sequence pointer must not be null");
        }

        const auto *sequence_bytes = static_cast<const unsigned char *>(sequence);
        const int sequence_size = static_cast<int>(std::strlen(reinterpret_cast<const char *>(sequence_bytes)));
        if (sequence_size <= 0)
        {
            return cpp_core::failMsg<int>(
                callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kBufferError),
                "Sequence must not be empty");
        }

        return cpp_bindings_windows::detail::readImpl(handle, buffer, buffer_size, timeout_ms, multiplier,
                                                      sequence_bytes, sequence_size, callback);
    }

} // extern "C"
