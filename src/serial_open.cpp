#include <cpp_core/interface/serial_open.h>
#include <cpp_core/result.hpp>
#include <cpp_core/strong_types.hpp>
#include <cpp_core/validation.hpp>

#include "detail/win32_helpers.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace
{
auto utf8ToWide(const char *utf8) -> std::wstring
{
    if (utf8 == nullptr || utf8[0] == '\0')
    {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (needed <= 0)
    {
        return {};
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), needed);
    if (written <= 0)
    {
        return {};
    }
    if (!out.empty() && out.back() == L'\0')
    {
        out.pop_back();
    }
    return out;
}

auto normalizePortPath(const wchar_t *port) -> std::wstring
{
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

auto applyLineSettings(HANDLE handle, int baudrate, int data_bits, cpp_core::Parity par,
                        cpp_core::StopBits sb) -> cpp_core::Status
{
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);

    if (GetCommState(handle, &dcb) == 0)
    {
        const DWORD err = GetLastError();
        return cpp_core::fail(cpp_core::StatusCodes::kGetStateError,
                              "GetCommState failed: " + cpp_bindings_windows::detail::win32ErrorToString(err));
    }

    dcb.BaudRate = static_cast<DWORD>(baudrate);
    dcb.ByteSize = static_cast<BYTE>(data_bits);

    dcb.fBinary = TRUE;
    dcb.fParity = (par != cpp_core::Parity::kNone) ? TRUE : FALSE;

    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    switch (par)
    {
    case cpp_core::Parity::kNone:
        dcb.Parity = NOPARITY;
        break;
    case cpp_core::Parity::kEven:
        dcb.Parity = EVENPARITY;
        break;
    case cpp_core::Parity::kOdd:
        dcb.Parity = ODDPARITY;
        break;
    default:
        return cpp_core::fail(cpp_core::StatusCodes::kSetStateError, "Invalid parity");
    }

    if (sb == cpp_core::StopBits::kOne)
    {
        dcb.StopBits = ONESTOPBIT;
    }
    else if (sb == cpp_core::StopBits::kTwo)
    {
        dcb.StopBits = TWOSTOPBITS;
    }

    if (SetCommState(handle, &dcb) == 0)
    {
        const DWORD err = GetLastError();
        return cpp_core::fail(cpp_core::StatusCodes::kSetStateError,
                              "SetCommState failed: " + cpp_bindings_windows::detail::win32ErrorToString(err));
    }

    COMMTIMEOUTS timeouts = {};
    if (SetCommTimeouts(handle, &timeouts) == 0)
    {
        const DWORD err = GetLastError();
        return cpp_core::fail(cpp_core::StatusCodes::kSetTimeoutError,
                              "SetCommTimeouts failed: " + cpp_bindings_windows::detail::win32ErrorToString(err));
    }

    return cpp_core::ok();
}
} // namespace

extern "C"
{
    MODULE_API auto serialOpen(void *port, int baudrate, int data_bits, int parity, int stop_bits,
                               ErrorCallbackT error_callback) -> intptr_t
    {
        const auto params_ok = cpp_core::validateOpenParams<intptr_t>(port, baudrate, data_bits, error_callback);
        if (params_ok < 0)
        {
            return params_ok;
        }

        const auto par = static_cast<cpp_core::Parity>(parity);

        // stop_bits: 0 or 1 = one stop bit (0 kept for backward compat), 2 = two stop bits
        if (stop_bits != static_cast<int>(cpp_core::StopBits::kOne) && stop_bits != 1 &&
            stop_bits != static_cast<int>(cpp_core::StopBits::kTwo))
        {
            return cpp_core::failMsg<intptr_t>(error_callback, cpp_core::StatusCodes::kSetStateError,
                                               "Invalid stop bits: must be 0, 1, or 2");
        }
        const auto sb = (stop_bits == static_cast<int>(cpp_core::StopBits::kTwo)) ? cpp_core::StopBits::kTwo
                                                                                   : cpp_core::StopBits::kOne;

        const auto *port_utf8 = static_cast<const char *>(port);
        std::wstring port_wide = utf8ToWide(port_utf8);
        if (port_wide.empty())
        {
            return cpp_core::failMsg<intptr_t>(error_callback, cpp_core::StatusCodes::kNotFoundError,
                                               "Port string is invalid or not valid UTF-8");
        }
        const std::wstring device_path = normalizePortPath(port_wide.c_str());

        cpp_bindings_windows::detail::UniqueHandle handle(
            CreateFileW(device_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr));

        if (!handle)
        {
            return cpp_bindings_windows::detail::failWin32<intptr_t>(error_callback,
                                                                     cpp_core::StatusCodes::kNotFoundError);
        }

        const auto settings = applyLineSettings(handle.get(), baudrate, data_bits, par, sb);
        if (!settings.has_value())
        {
            return static_cast<intptr_t>(cpp_core::toCStatus(settings, error_callback));
        }

        PurgeComm(handle.get(), PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

        const intptr_t out = reinterpret_cast<intptr_t>(handle.get());
        if (out <= 0)
        {
            return cpp_core::failMsg<intptr_t>(error_callback, cpp_core::StatusCodes::kInvalidHandleError,
                                               "Invalid handle generated");
        }
        return reinterpret_cast<intptr_t>(handle.release());
    }

} // extern "C"
