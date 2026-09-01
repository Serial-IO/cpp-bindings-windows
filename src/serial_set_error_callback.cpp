#include <cpp_core/interface/serial_set_error_callback.h>

#include "detail/common_types.hpp"

extern "C"
{

    MODULE_API void serialSetErrorCallback(ErrorCallbackT error_callback)
    {
        cpp_bindings_windows::detail::g_error_callback.store(error_callback, std::memory_order_release);
    }

} // extern "C"
