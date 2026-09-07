#include <cpp_core/serial.h>
#include <cpp_core/status_code.h>

#include "detail/apply_flow_control.hpp"
#include "detail/matches_suffix.hpp"

#include <array>
#include <cstring>
#include <limits>
#include <string_view>

#include <gtest/gtest.h>

namespace
{
constexpr auto kConfig = cpp_core::SerialConfig::make<9600, cpp_core::DataBits::kEight>();
constexpr auto kTimeout = cpp_core::SerialTimeoutConfig::make<10, 1>();
constexpr int kTimeoutError = static_cast<int>(cpp_core::StatusCode::Configuration::kSetTimeoutError);
int g_error = 0;
void captureError(int code, const char *)
{
    g_error = code;
}
void portEvent(cpp_core::PortEvent, const char *)
{
}
} // namespace

TEST(SerialV3ApiTest, RejectsNullAndInvalidConfigurationsBeforeOpening)
{
    EXPECT_EQ(serialOpen("COM99999", nullptr), static_cast<int>(cpp_core::StatusCode::Control::kSetStateError));
    const auto check = [](cpp_core::SerialConfig config, int expected) {
        g_error = 0;
        EXPECT_EQ(serialOpen("COM99999", &config, captureError), expected);
        EXPECT_EQ(g_error, expected);
    };
    auto config = kConfig;
    config.parity = static_cast<cpp_core::Parity>(99);
    check(config, static_cast<int>(cpp_core::StatusCode::Configuration::kSetParityError));
    config = kConfig;
    config.stop_bits = static_cast<cpp_core::StopBits>(1);
    check(config, static_cast<int>(cpp_core::StatusCode::Configuration::kSetStopBitsError));
    config = kConfig;
    config.flow_mode = static_cast<cpp_core::FlowControl>(99);
    check(config, static_cast<int>(cpp_core::StatusCode::Configuration::kSetFlowControlError));
}

TEST(SerialV3ApiTest, RejectsNullNegativeAndOverflowingTimeouts)
{
    std::array<std::uint8_t, 4> buffer{};
    const auto check = [&](const cpp_core::SerialTimeoutConfig *timeout) {
        g_error = 0;
        EXPECT_EQ(serialRead(-1, buffer.data(), 4, timeout, captureError), kTimeoutError);
        EXPECT_EQ(g_error, kTimeoutError);
        g_error = 0;
        EXPECT_EQ(serialWrite(-1, buffer.data(), 4, timeout, captureError), kTimeoutError);
        EXPECT_EQ(g_error, kTimeoutError);
        EXPECT_EQ(serialReadUntilSequence(-1, buffer.data(), 4, timeout, buffer.data(), 1), kTimeoutError);
    };
    check(nullptr);
    for (const auto timeout : {cpp_core::SerialTimeoutConfig{-1, 1}, {1, -1}, {std::numeric_limits<int>::max(), 2}})
    {
        check(&timeout);
    }
    const cpp_core::SerialTimeoutConfig zero{0, std::numeric_limits<int>::max()};
    EXPECT_EQ(serialRead(-1, buffer.data(), 4, &zero),
              static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST(SerialV3ApiTest, BinaryTerminatorsUseExplicitLength)
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

TEST(SerialV3ApiTest, FlowControlTransitionsClearPreviousModes)
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

TEST(SerialV3ApiTest, TypedGettersPreserveNegativeErrors)
{
    constexpr int expected = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
    EXPECT_EQ(cpp_core::toInt(serialGetDataBits(-1)), expected);
    EXPECT_EQ(cpp_core::toInt(serialGetParity(-1)), expected);
    EXPECT_EQ(cpp_core::toInt(serialGetStopBits(-1)), expected);
    EXPECT_EQ(cpp_core::toInt(serialGetFlowControl(-1)), expected);
    EXPECT_EQ(serialWaitForDrain(-1), expected);
}

TEST(SerialV3ApiTest, MetadataDescribesLoadedBinding)
{
    meta(nullptr);
    cpp_core::Meta info{};
    meta(&info);
    EXPECT_STREQ(info.version_string, CPP_BINDINGS_WINDOWS_TEST_VERSION);
    ASSERT_NE(info.git_commit_hash_full, nullptr);
    EXPECT_EQ(std::strlen(info.git_commit_hash_full), 40U);
    ASSERT_NE(info.git_commit_hash_short, nullptr);
    EXPECT_TRUE(std::string_view(info.git_commit_hash_full).starts_with(info.git_commit_hash_short));
    EXPECT_NE(info.prerelease, nullptr);
    EXPECT_NE(info.git_tag, nullptr);
    EXPECT_NE(info.git_commit_date, nullptr);
    EXPECT_NE(info.git_branch, nullptr);
}

TEST(SerialV3ApiTest, EventCallbackCanBeReplacedAndCleared)
{
    EXPECT_EQ(serialSetEventCallback(nullptr), 0);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(serialSetEventCallback(portEvent), 0);
        EXPECT_EQ(serialSetEventCallback(portEvent), 0);
        EXPECT_EQ(serialSetEventCallback(nullptr), 0);
    }
}
