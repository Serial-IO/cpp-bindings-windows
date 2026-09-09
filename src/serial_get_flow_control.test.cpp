#include <cpp_core/interface/serial_get_flow_control.h>
#include <cpp_core/status_code.h>

#include <gtest/gtest.h>

TEST(SerialGetFlowControlTest, PreservesNegativeHandleError)
{
    constexpr int expected = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
    EXPECT_EQ(cpp_core::toInt(serialGetFlowControl(-1)), expected);
}
