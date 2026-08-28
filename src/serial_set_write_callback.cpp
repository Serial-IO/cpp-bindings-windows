#include <cpp_core/interface/serial_set_write_callback.h>

#include "detail/handle_types.hpp"

extern "C"
{

    MODULE_API void serialSetWriteCallback(void (*callback_fn)(int bytes_written))
    {
        cpp_bindings_windows::detail::g_write_callback.store(callback_fn, std::memory_order_release);
    }

} // extern "C"
