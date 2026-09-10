#include <cpp_core/interface/serial_get_parity.h>
#include <cpp_core/status_code.h>

#include <gtest/gtest.h>

TEST(SerialGetParityTest, PreservesNegativeHandleError)
{
    constexpr int expected = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
    EXPECT_EQ(cpp_core::toInt(serialGetParity(-1)), expected);
}
