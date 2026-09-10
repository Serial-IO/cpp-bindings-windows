#include <cpp_core/interface/serial_read_until_sequence.h>
#include <cpp_core/status_code.h>

#include "detail/matches_suffix.hpp"

#include <array>
#include <limits>

#include <gtest/gtest.h>

namespace
{
constexpr auto kTimeout = cpp_core::SerialTimeoutConfig::make<10, 1>();
} // namespace

TEST(SerialReadUntilSequenceTest, RejectsNullNegativeAndOverflowingTimeouts)
{
    constexpr int kTimeoutError = static_cast<int>(cpp_core::StatusCode::Configuration::kSetTimeoutError);
    std::array<std::uint8_t, 4> buffer{};
    const auto check = [&](const cpp_core::SerialTimeoutConfig *timeout) {
        EXPECT_EQ(serialReadUntilSequence(-1, buffer.data(), 4, timeout, buffer.data(), 1), kTimeoutError);
    };
    check(nullptr);
    for (const auto timeout : {cpp_core::SerialTimeoutConfig{-1, 1}, {1, -1}, {std::numeric_limits<int>::max(), 2}})
    {
        check(&timeout);
    }
}

TEST(SerialReadUntilSequenceTest, BinaryTerminatorsUseExplicitLength)
{
    constexpr std::array<std::uint8_t, 4> data{'a', 0, 'b', 'c'};
    constexpr std::array<std::uint8_t, 3> sequence{0, 'b', 'x'};
    EXPECT_TRUE(cpp_bindings_windows::detail::matchesSuffix(data.data(), 3, sequence.data(), 2));
    EXPECT_FALSE(cpp_bindings_windows::detail::matchesSuffix(data.data(), 4, sequence.data(), 2));
    EXPECT_FALSE(cpp_bindings_windows::detail::matchesSuffix(data.data(), 3, sequence.data(), 3));
    std::array<std::uint8_t, 8> buffer{};
    constexpr int invalid_handle = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
    // A zero byte is a valid nonempty terminator and must reach handle validation.
    EXPECT_EQ(serialReadUntilSequence(-1, buffer.data(), 8, &kTimeout, sequence.data(), 1), invalid_handle);
    EXPECT_EQ(serialReadUntilSequence(-1, buffer.data(), 8, &kTimeout, sequence.data(), -1),
              static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
}
