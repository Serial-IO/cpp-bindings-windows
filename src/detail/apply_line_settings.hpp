#pragma once

#include "win32_error_to_string.hpp"

#include <cpp_core/result.hpp>
#include <cpp_core/strong_types.hpp>

namespace cpp_bindings_windows::detail
{
inline auto applyLineSettings(HANDLE handle, int baudrate, int data_bits, cpp_core::Parity parity_value,
                              cpp_core::StopBits stop_bits_value) -> cpp_core::Status
{
    DCB serial_settings = {};
    serial_settings.DCBlength = sizeof(DCB);

    if (GetCommState(handle, &serial_settings) == 0)
    {
        const DWORD error = GetLastError();
        return cpp_core::fail(cpp_core::StatusCode::Control::kGetStateError,
                              "GetCommState failed: " + win32ErrorToString(error));
    }

    serial_settings.BaudRate = static_cast<DWORD>(baudrate);
    serial_settings.ByteSize = static_cast<BYTE>(data_bits);

    serial_settings.fBinary = TRUE;
    serial_settings.fParity = (parity_value != cpp_core::Parity::kNone) ? TRUE : FALSE;
    serial_settings.fOutxCtsFlow = FALSE;
    serial_settings.fOutxDsrFlow = FALSE;
    serial_settings.fDtrControl = DTR_CONTROL_ENABLE;
    serial_settings.fDsrSensitivity = FALSE;
    serial_settings.fTXContinueOnXoff = TRUE;
    serial_settings.fOutX = FALSE;
    serial_settings.fInX = FALSE;
    serial_settings.fRtsControl = RTS_CONTROL_ENABLE;

    switch (parity_value)
    {
    case cpp_core::Parity::kNone:
        serial_settings.Parity = NOPARITY;
        break;
    case cpp_core::Parity::kEven:
        serial_settings.Parity = EVENPARITY;
        break;
    case cpp_core::Parity::kOdd:
        serial_settings.Parity = ODDPARITY;
        break;
    default:
        return cpp_core::fail(cpp_core::StatusCode::Control::kSetStateError, "Invalid parity");
    }

    if (stop_bits_value == cpp_core::StopBits::kOne)
    {
        serial_settings.StopBits = ONESTOPBIT;
    }
    else if (stop_bits_value == cpp_core::StopBits::kTwo)
    {
        serial_settings.StopBits = TWOSTOPBITS;
    }

    if (SetCommState(handle, &serial_settings) == 0)
    {
        const DWORD error = GetLastError();
        return cpp_core::fail(cpp_core::StatusCode::Control::kSetStateError,
                              "SetCommState failed: " + win32ErrorToString(error));
    }

    COMMTIMEOUTS communication_timeouts = {};
    if (SetCommTimeouts(handle, &communication_timeouts) == 0)
    {
        const DWORD error = GetLastError();
        return cpp_core::fail(cpp_core::StatusCode::Configuration::kSetTimeoutError,
                              "SetCommTimeouts failed: " + win32ErrorToString(error));
    }

    return cpp_core::ok();
}
} // namespace cpp_bindings_windows::detail
