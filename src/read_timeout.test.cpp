#include "detail/read_timeout.hpp"

#include <gtest/gtest.h>

namespace cpp_bindings_windows::detail
{
TEST(ReadTimeoutTest, UsesBaseTimeoutForFirstRead)
{
    EXPECT_EQ(readTimeout(250, 0, true, false), 250);
}

TEST(ReadTimeoutTest, RawReadWithZeroMultiplierDoesNotWaitForMoreData)
{
    EXPECT_EQ(readTimeout(250, 0, false, false), 0);
}

TEST(ReadTimeoutTest, TerminatedReadWithZeroMultiplierKeepsWaitingForTerminator)
{
    EXPECT_EQ(readTimeout(250, 0, false, true), 250);
}

TEST(ReadTimeoutTest, TerminatedReadStillAppliesPositiveMultiplier)
{
    EXPECT_EQ(readTimeout(250, 3, false, true), 750);
}
} // namespace cpp_bindings_windows::detail
