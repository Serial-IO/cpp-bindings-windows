#include <cpp_core/interface/serial_wait_for_drain.h>
#include <cpp_core/status_code.h>

#include <gtest/gtest.h>

TEST(SerialWaitForDrainTest, PreservesNegativeHandleError)
{
    constexpr int expected = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
    EXPECT_EQ(serialWaitForDrain(-1), expected);
}
