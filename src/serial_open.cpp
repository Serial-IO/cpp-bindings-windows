#include <cpp_core/interface/serial_open.h>
#include <cpp_core/status_codes.h>

#include "detail/win32_helpers.hpp"

#include <string>

namespace
{
auto normalizePortPath(const wchar_t *port) -> std::wstring
{
    // Accept:
    // - "COM3"
    // - "\\\\.\\COM10"
    // Always produce a path that works for COM1..COM*.
    std::wstring p(port);
    if (p.rfind(L"\\\\.\\", 0) == 0)
    {
        return p;
    }
    if (p.rfind(L"COM", 0) == 0 || p.rfind(L"com", 0) == 0)
    {
        return L"\\\\.\\" + p;
    }
    return p;
}

struct ApplyLineSettingsResult
{
    cpp_core::StatusCodes code;
    std::string message;
};

auto applyLineSettings(HANDLE handle, int baudrate, int data_bits, int parity, int stop_bits) -> ApplyLineSettingsResult
{
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);

    if (GetCommState(handle, &dcb) == 0)
    {
        const DWORD err = GetLastError();
        return {.code = cpp_core::StatusCodes::kGetStateError,
                .message = "GetCommState failed: " + cpp_bindings_windows::detail::win32ErrorToString(err)};
    }

    dcb.BaudRate = static_cast<DWORD>(baudrate);
    dcb.ByteSize = static_cast<BYTE>(data_bits);

    dcb.fBinary = TRUE;
    dcb.fParity = (parity != 0) ? TRUE : FALSE;

    // Disable flow control by default (matches the Linux impl's raw mode
    // behavior)
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    switch (parity)
    {
    case 0:
        dcb.Parity = NOPARITY;
        break;
    case 1:
        dcb.Parity = EVENPARITY;
        break;
    case 2:
        dcb.Parity = ODDPARITY;
        break;
    default:
        SetLastError(ERROR_INVALID_PARAMETER);
        return {.code = cpp_core::StatusCodes::kSetStateError, .message = "Invalid parity"};
    }

    // stop_bits mapping:
    //   0 or 1 = 1 stop bit (0 kept for backward compatibility with callers using
    //   "default") 2      = 2 stop bits
    if (stop_bits == 0 || stop_bits == 1)
    {
        dcb.StopBits = ONESTOPBIT;
    }
    else if (stop_bits == 2)
    {
        dcb.StopBits = TWOSTOPBITS;
    }
    else
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return {.code = cpp_core::StatusCodes::kSetStateError, .message = "Invalid stop bits: must be 0, 1, or 2"};
    }

    if (SetCommState(handle, &dcb) == 0)
    {
        const DWORD err = GetLastError();
        return {.code = cpp_core::StatusCodes::kSetStateError,
                .message = "SetCommState failed: " + cpp_bindings_windows::detail::win32ErrorToString(err)};
    }

    // Use overlapped operations + explicit waits in read/write, so keep
    // COMMTIMEOUTS as 0.
    COMMTIMEOUTS timeouts = {};
    if (SetCommTimeouts(handle, &timeouts) == 0)
    {
        const DWORD err = GetLastError();
        return {.code = cpp_core::StatusCodes::kSetTimeoutError,
                .message = "SetCommTimeouts failed: " + cpp_bindings_windows::detail::win32ErrorToString(err)};
    }

    return {.code = cpp_core::StatusCodes::kSuccess, .message = ""};
}
} // namespace

extern "C"
{
    MODULE_API auto serialOpen(void *port, int baudrate, int data_bits, int parity, int stop_bits,
                               ErrorCallbackT error_callback) -> intptr_t
    {
        if (port == nullptr)
        {
            return cpp_bindings_windows::detail::failMsg<intptr_t>(
                error_callback, cpp_core::StatusCodes::kNotFoundError, "Port parameter is nullptr");
        }

        if (baudrate < 300)
        {
            return cpp_bindings_windows::detail::failMsg<intptr_t>(
                error_callback, cpp_core::StatusCodes::kSetStateError, "Invalid baudrate: must be >= 300");
        }

        if (data_bits < 5 || data_bits > 8)
        {
            return cpp_bindings_windows::detail::failMsg<intptr_t>(
                error_callback, cpp_core::StatusCodes::kSetStateError, "Invalid data bits: must be 5-8");
        }

        // Windows: port is documented as const wchar_t*
        const auto *port_path = static_cast<const wchar_t *>(port);
        const std::wstring device_path = normalizePortPath(port_path);

        cpp_bindings_windows::detail::UniqueHandle handle(
            CreateFileW(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));

        if (!handle.valid())
        {
            return cpp_bindings_windows::detail::failWin32<intptr_t>(error_callback,
                                                                     cpp_core::StatusCodes::kNotFoundError);
        }

        const ApplyLineSettingsResult settings =
            applyLineSettings(handle.get(), baudrate, data_bits, parity, stop_bits);
        if (settings.code != cpp_core::StatusCodes::kSuccess)
        {
            const char *msg = settings.message.empty() ? "Failed to configure serial port" : settings.message.c_str();
            return cpp_bindings_windows::detail::failMsg<intptr_t>(error_callback, settings.code, msg);
        }

        // Clear buffers (best-effort)
        PurgeComm(handle.get(), PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

        // Validate before releasing ownership to avoid any chance of leaking the handle.
        const intptr_t out = reinterpret_cast<intptr_t>(handle.get());
        if (out <= 0)
        {
            // Extremely unlikely, but keep the API contract: success -> positive
            // handle.
            return cpp_bindings_windows::detail::failMsg<intptr_t>(
                error_callback, cpp_core::StatusCodes::kInvalidHandleError, "Invalid handle generated");
        }
        return reinterpret_cast<intptr_t>(handle.release());
    }

} // extern "C"
