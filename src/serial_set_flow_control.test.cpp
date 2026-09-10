#include "detail/apply_flow_control.hpp"

#include <gtest/gtest.h>

TEST(SerialSetFlowControlTest, FlowControlTransitionsClearPreviousModes)
{
    DCB settings{};
    settings.BaudRate = 115200;
    settings.ByteSize = 8;
    cpp_bindings_windows::detail::applyFlowControl(settings, cpp_core::FlowControl::kRtsCts);
    EXPECT_EQ(static_cast<int>(settings.fOutxCtsFlow), TRUE);
    EXPECT_EQ(static_cast<int>(settings.fRtsControl), RTS_CONTROL_HANDSHAKE);
    EXPECT_EQ(static_cast<int>(settings.fOutX), FALSE);
    EXPECT_EQ(static_cast<int>(settings.fInX), FALSE);
    cpp_bindings_windows::detail::applyFlowControl(settings, cpp_core::FlowControl::kXonXoff);
    EXPECT_EQ(static_cast<int>(settings.fOutxCtsFlow), FALSE);
    EXPECT_EQ(static_cast<int>(settings.fRtsControl), RTS_CONTROL_ENABLE);
    EXPECT_EQ(static_cast<int>(settings.fOutX), TRUE);
    EXPECT_EQ(static_cast<int>(settings.fInX), TRUE);
    EXPECT_EQ(settings.XonChar, 0x11);
    EXPECT_EQ(settings.XoffChar, 0x13);
    cpp_bindings_windows::detail::applyFlowControl(settings, cpp_core::FlowControl::kNone);
    EXPECT_EQ(static_cast<int>(settings.fOutxCtsFlow), FALSE);
    EXPECT_EQ(static_cast<int>(settings.fOutX), FALSE);
    EXPECT_EQ(static_cast<int>(settings.fInX), FALSE);
    EXPECT_EQ(settings.BaudRate, 115200U);
    EXPECT_EQ(settings.ByteSize, 8);
}
