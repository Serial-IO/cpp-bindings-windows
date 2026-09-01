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
    MODULE_API auto serialOpen(void *port, int baudrate, int data_bits, int parity, int stop_bits,
                               ErrorCallbackT error_callback) -> intptr_t
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        const auto parameter_status = cpp_core::validateOpenParams<intptr_t>(port, baudrate, data_bits, callback);
        if (parameter_status < 0)
        {
            return parameter_status;
        }

        if (parity < static_cast<int>(cpp_core::Parity::kNone) || parity > static_cast<int>(cpp_core::Parity::kOdd))
        {
            return cpp_core::failMsg<intptr_t>(callback, cpp_core::StatusCode::Control::kSetStateError,
                                               "Invalid parity: must be 0, 1, or 2");
        }
        const auto parity_value = static_cast<cpp_core::Parity>(parity);

        // stop_bits: 0 or 1 = one stop bit (0 kept for backward compat), 2 = two stop bits
        if (stop_bits != static_cast<int>(cpp_core::StopBits::kOne) && stop_bits != 1 &&
            stop_bits != static_cast<int>(cpp_core::StopBits::kTwo))
        {
            return cpp_core::failMsg<intptr_t>(callback, cpp_core::StatusCode::Control::kSetStateError,
                                               "Invalid stop bits: must be 0, 1, or 2");
        }
        const auto stop_bits_value = (stop_bits == static_cast<int>(cpp_core::StopBits::kTwo))
                                         ? cpp_core::StopBits::kTwo
                                         : cpp_core::StopBits::kOne;

        const auto *port_utf8 = static_cast<const char *>(port);
        std::wstring port_wide = cpp_bindings_windows::detail::utf8ToWide(port_utf8);
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

        const auto settings = cpp_bindings_windows::detail::applyLineSettings(handle.get(), baudrate, data_bits,
                                                                              parity_value, stop_bits_value);
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
