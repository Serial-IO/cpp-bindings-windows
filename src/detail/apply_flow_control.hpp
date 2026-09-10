#pragma once

#include "windows.hpp"
#include <cpp_core/strong_types.hpp>

namespace cpp_bindings_windows::detail
{
inline void applyFlowControl(DCB &settings, cpp_core::FlowControl mode)
{
    settings.fOutxCtsFlow = FALSE;
    settings.fRtsControl = RTS_CONTROL_ENABLE;
    settings.fOutX = FALSE;
    settings.fInX = FALSE;
    switch (mode)
    {
    case cpp_core::FlowControl::kRtsCts:
        settings.fOutxCtsFlow = TRUE;
        settings.fRtsControl = RTS_CONTROL_HANDSHAKE;
        break;
    case cpp_core::FlowControl::kXonXoff:
        settings.fOutX = TRUE;
        settings.fInX = TRUE;
        settings.XonChar = 0x11;
        settings.XoffChar = 0x13;
        settings.XonLim = 2048;
        settings.XoffLim = 512;
        break;
    default:
        break;
    }
}
} // namespace cpp_bindings_windows::detail
