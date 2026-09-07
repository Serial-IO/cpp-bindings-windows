#include <cpp_core/interface/serial_read_until_sequence.h>

#include "detail/read_impl.hpp"

extern "C"
{

    MODULE_API auto serialReadUntilSequence(int64_t handle, std::uint8_t *buffer, int buffer_size,
                                            const cpp_core::SerialTimeoutConfig *timeout_config,
                                            const std::uint8_t *sequence, int sequence_size,
                                            ErrorCallbackT error_callback) -> int
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        if (sequence == nullptr || sequence_size <= 0)
        {
            return cpp_core::failMsg<int>(
                callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kBufferError),
                "Sequence must not be null and sequence_size must be positive");
        }

        return cpp_bindings_windows::detail::readImpl(handle, buffer, buffer_size, timeout_config, sequence,
                                                      sequence_size, callback);
    }

} // extern "C"
