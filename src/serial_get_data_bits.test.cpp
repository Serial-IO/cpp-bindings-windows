#include <cpp_core/interface/serial_get_data_bits.h>
#include <cpp_core/status_code.h>

#include <gtest/gtest.h>

TEST(SerialGetDataBitsTest, PreservesNegativeHandleError)
{
    constexpr int expected = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
    EXPECT_EQ(cpp_core::toInt(serialGetDataBits(-1)), expected);
}
