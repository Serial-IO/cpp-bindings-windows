#include <cpp_core/interface/serial_write.h>
#include <cpp_core/status_code.h>

#include <array>
#include <cstring>
#include <limits>

#include <gtest/gtest.h>

#include "test_helpers/error_capture.hpp"

class SerialWriteTest : public ::testing::Test
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

TEST_F(SerialWriteTest, WriteNullBuffer)
{
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialWrite(1, nullptr, 10, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
    EXPECT_NE(error_capture.last_message.find("buffer"), std::string::npos);
}

TEST_F(SerialWriteTest, WriteZeroBufferSize)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result =
        serialWrite(1, reinterpret_cast<const std::uint8_t *>(buffer.data()), 0, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
}

TEST_F(SerialWriteTest, WriteNegativeBufferSize)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result =
        serialWrite(1, reinterpret_cast<const std::uint8_t *>(buffer.data()), -1, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
}

TEST_F(SerialWriteTest, WriteInvalidHandleZero)
{
    const char *buffer = "test";
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialWrite(0, reinterpret_cast<const std::uint8_t *>(buffer), static_cast<int>(strlen(buffer)),
                             &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialWriteTest, WriteInvalidHandleNegative)
{
    const char *buffer = "test";
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialWrite(-1, reinterpret_cast<const std::uint8_t *>(buffer), static_cast<int>(strlen(buffer)),
                             &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialWriteTest, WriteHandleAboveIntMaxIsNotRejectedByRangeValidation)
{
    const char *buffer = "test";
    auto too_large = static_cast<int64_t>(std::numeric_limits<int>::max()) + 1;
    const cpp_core::SerialTimeoutConfig timeout_config{100, 0};
    int result = serialWrite(too_large, reinterpret_cast<const std::uint8_t *>(buffer),
                             static_cast<int>(strlen(buffer)), &timeout_config, error_callback);

    EXPECT_NE(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}

TEST_F(SerialWriteTest, WriteEmptyStringZeroSize)
{
    const char *empty = "";
    const cpp_core::SerialTimeoutConfig timeout_config{0, 0};
    int result = serialWrite(1, reinterpret_cast<const std::uint8_t *>(empty), 0, &timeout_config, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Io::kBufferError));
}

TEST_F(SerialWriteTest, WriteNoErrorCallback)
{
    std::array<char, 10> buffer{};
    const cpp_core::SerialTimeoutConfig timeout_config{0, 0};
    int result = serialWrite(0, reinterpret_cast<const std::uint8_t *>(buffer.data()), 1, &timeout_config, nullptr);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError));
}
