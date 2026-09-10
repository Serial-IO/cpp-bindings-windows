#include <cpp_core/interface/serial_read.h>
#include <cpp_core/status_code.h>

#include <array>
#include <limits>

#include <gtest/gtest.h>

#include "test_helpers/error_capture.hpp"

class SerialReadTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        ErrorCapture::instance = &error_capture;
        error_callback = &ErrorCapture::callback;
    }

    void TearDown() override
    {
        ErrorCapture::instance = nullptr;
    }

    ErrorCapture error_capture;
    ErrorCallbackT error_callback = nullptr;
};

TEST_F(SerialReadTest, ReadNullBuffer)
{
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(1, nullptr, 10, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
    EXPECT_NE(error_capture.last_message.find("buffer"), std::string::npos);
}

TEST_F(SerialReadTest, ReadZeroBufferSize)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(1, reinterpret_cast<std::uint8_t *>(buffer.data()), 0, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
}

TEST_F(SerialReadTest, ReadNegativeBufferSize)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(1, reinterpret_cast<std::uint8_t *>(buffer.data()), -1, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
}

TEST_F(SerialReadTest, ReadInvalidHandleZero)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(0, reinterpret_cast<std::uint8_t *>(buffer.data()), static_cast<int>(buffer.size()),
                            &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialReadTest, ReadInvalidHandleNegative)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(-1, reinterpret_cast<std::uint8_t *>(buffer.data()), static_cast<int>(buffer.size()),
                            &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialReadTest, ReadHandleAboveIntMaxIsNotRejectedByRangeValidation)
{
    std::array<char, 10> buffer{};
    auto too_large = static_cast<int64_t>(std::numeric_limits<int>::max()) + 1;
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(too_large, reinterpret_cast<std::uint8_t *>(buffer.data()), static_cast<int>(buffer.size()),
                            &timeout_config, error_callback);

    EXPECT_NE(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialReadTest, ReadNoErrorCallback)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialRead(0, reinterpret_cast<std::uint8_t *>(buffer.data()), static_cast<int>(buffer.size()),
                            &timeout_config, nullptr);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialReadTest, RejectsNullNegativeAndOverflowingTimeouts)
{
    constexpr int kTimeoutError = static_cast<int>(cpp_core::StatusCode::Configuration::kSetTimeoutError);
    std::array<std::uint8_t, 4> buffer{};
    const auto check = [&](const cpp_core::SerialTimeoutConfig *timeout) {
        error_capture.last_code = 0;
        EXPECT_EQ(serialRead(-1, buffer.data(), 4, timeout, error_callback), kTimeoutError);
        EXPECT_EQ(error_capture.last_code, kTimeoutError);
    };
    check(nullptr);
    for (const auto timeout : {cpp_core::SerialTimeoutConfig{-1, 1}, {1, -1}, {std::numeric_limits<int>::max(), 2}})
    {
        check(&timeout);
    }
}

TEST_F(SerialReadTest, AcceptsZeroTimeoutWithMaximumMultiplier)
{
    std::array<std::uint8_t, 4> buffer{};
    const cpp_core::SerialTimeoutConfig zero{0, std::numeric_limits<int>::max()};
    EXPECT_EQ(serialRead(-1, buffer.data(), 4, &zero), static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}
