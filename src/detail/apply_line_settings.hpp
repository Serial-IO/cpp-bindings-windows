#pragma once

#include "win32_error_to_string.hpp"

#include <cpp_core/result.hpp>
#include <cpp_core/strong_types.hpp>

namespace cpp_bindings_windows::detail
{
inline auto applyLineSettings(HANDLE handle, int baudrate, int data_bits, cpp_core::Parity parity_value,
                              cpp_core::StopBits stop_bits_value) -> cpp_core::Status
{
    DCB dcb = {};
    dcb.DCBlength = sizeof(DCB);

    if (GetCommState(handle, &dcb) == 0)
    {
        const DWORD error = GetLastError();
        return cpp_core::fail(cpp_core::StatusCode::Control::kGetStateError,
                              "GetCommState failed: " + win32ErrorToString(error));
    }

    dcb.BaudRate = static_cast<DWORD>(baudrate);
    dcb.ByteSize = static_cast<BYTE>(data_bits);

    dcb.fBinary = TRUE;
    dcb.fParity = (parity_value != cpp_core::Parity::kNone) ? TRUE : FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fDsrSensitivity = FALSE;
    dcb.fTXContinueOnXoff = TRUE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    switch (parity_value)
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
        return cpp_core::fail(cpp_core::StatusCode::Control::kSetStateError, "Invalid parity");
    }

    if (stop_bits_value == cpp_core::StopBits::kOne)
    {
        dcb.StopBits = ONESTOPBIT;
    }
    else if (stop_bits_value == cpp_core::StopBits::kTwo)
    {
        dcb.StopBits = TWOSTOPBITS;
    }

    if (SetCommState(handle, &dcb) == 0)
    {
        const DWORD error = GetLastError();
        return cpp_core::fail(cpp_core::StatusCode::Control::kSetStateError,
                              "SetCommState failed: " + win32ErrorToString(error));
    }

    COMMTIMEOUTS timeouts = {};
    if (SetCommTimeouts(handle, &timeouts) == 0)
    {
        const DWORD error = GetLastError();
        return cpp_core::fail(cpp_core::StatusCode::Configuration::kSetTimeoutError,
                              "SetCommTimeouts failed: " + win32ErrorToString(error));
    }

    return cpp_core::ok();
}
} // namespace cpp_bindings_windows::detail
