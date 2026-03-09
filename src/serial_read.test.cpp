#include <cpp_core/interface/serial_read.h>
#include <cpp_core/status_codes.h>

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
    int result = serialRead(1, nullptr, 10, 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
    EXPECT_NE(error_capture.last_message.find("buffer"), std::string::npos);
}

TEST_F(SerialReadTest, ReadZeroBufferSize)
{
    std::array<char, 10> buffer{};
    int result = serialRead(1, buffer.data(), 0, 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
}

TEST_F(SerialReadTest, ReadNegativeBufferSize)
{
    std::array<char, 10> buffer{};
    int result = serialRead(1, buffer.data(), -1, 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
}

TEST_F(SerialReadTest, ReadInvalidHandleZero)
{
    std::array<char, 10> buffer{};
    int result = serialRead(0, buffer.data(), static_cast<int>(buffer.size()), 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialReadTest, ReadInvalidHandleNegative)
{
    std::array<char, 10> buffer{};
    int result = serialRead(-1, buffer.data(), static_cast<int>(buffer.size()), 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialReadTest, ReadInvalidHandleTooLarge)
{
    std::array<char, 10> buffer{};
    auto too_large = static_cast<int64_t>(std::numeric_limits<int>::max()) + 1;
    int result = serialRead(too_large, buffer.data(), static_cast<int>(buffer.size()), 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialReadTest, ReadNoErrorCallback)
{
    std::array<char, 10> buffer{};
    int result = serialRead(0, buffer.data(), static_cast<int>(buffer.size()), 100, 0, nullptr);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}
