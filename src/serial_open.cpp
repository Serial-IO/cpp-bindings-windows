#include <cpp_core/interface/serial_open.h>
#include <cpp_core/result.hpp>
#include <cpp_core/strong_types.hpp>
#include <cpp_core/validation.hpp>

#include "detail/apply_line_settings.hpp"
#include "detail/effective_error_callback.hpp"
#include "detail/fail_win32.hpp"
#include "detail/handle_types.hpp"
#include "detail/normalize_port_path.hpp"
#include "detail/register_opened_handle.hpp"
#include "detail/utf8_to_wide.hpp"

#include <string>

extern "C"
{
    MODULE_API auto serialOpen(const char *port, const cpp_core::SerialConfig *config, ErrorCallbackT error_callback)
        -> intptr_t
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        const auto parameter_status = cpp_core::validateOpenParams<intptr_t>(port, config, callback);
        if (parameter_status < 0)
        {
            return parameter_status;
        }

        std::wstring port_wide = cpp_bindings_windows::detail::utf8ToWide(port);
        if (port_wide.empty())
        {
            return cpp_core::failMsg<intptr_t>(callback, cpp_core::StatusCode::Connection::kNotFoundError,
                                               "Port string is invalid or not valid UTF-8");
        }
        const std::wstring device_path = cpp_bindings_windows::detail::normalizePortPath(port_wide);

        const HANDLE raw_handle = CreateFileW(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);

        // CreateFileW returns INVALID_HANDLE_VALUE on failure, normalize to nullptr
        // so UniqueHandle (sentinel = nullptr) treats it as invalid.
        cpp_bindings_windows::detail::UniqueHandle handle((raw_handle == INVALID_HANDLE_VALUE) ? nullptr : raw_handle);

        if (!handle)
        {
            return cpp_bindings_windows::detail::failWin32<intptr_t>(callback,
                                                                     cpp_core::StatusCode::Connection::kNotFoundError);
        }

        const auto settings = cpp_bindings_windows::detail::applyLineSettings(handle.get(), *config);
        if (!settings.has_value())
        {
            return static_cast<intptr_t>(cpp_core::toCStatus(settings, callback));
        }

        PurgeComm(handle.get(), PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

        const intptr_t serial_handle = reinterpret_cast<intptr_t>(handle.get());
        if (serial_handle <= 0)
        {
            return cpp_core::failMsg<intptr_t>(callback, cpp_core::StatusCode::Connection::kInvalidHandleError,
                                               "Invalid handle generated");
        }
        const HANDLE opened_handle = handle.release();
        cpp_bindings_windows::detail::registerOpenedHandle(opened_handle);
        return reinterpret_cast<intptr_t>(opened_handle);
    }

} // extern "C"
