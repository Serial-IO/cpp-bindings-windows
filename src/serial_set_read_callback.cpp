#include <cpp_core/interface/serial_set_read_callback.h>

#include "detail/handle_types.hpp"

extern "C"
{

    MODULE_API void serialSetReadCallback(void (*callback_function)(int bytes_read))
    {
        cpp_bindings_windows::detail::g_read_callback.store(callback_function, std::memory_order_release);
    }

} // extern "C"
