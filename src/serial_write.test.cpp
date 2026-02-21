#include <cpp_core/interface/serial_write.h>
#include <cpp_core/status_codes.h>

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
    int result = serialWrite(1, nullptr, 10, 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
    EXPECT_NE(error_capture.last_message.find("buffer"), std::string::npos);
}

TEST_F(SerialWriteTest, WriteZeroBufferSize)
{
    std::array<char, 10> buffer{};
    int result = serialWrite(1, buffer.data(), 0, 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
}

TEST_F(SerialWriteTest, WriteNegativeBufferSize)
{
    std::array<char, 10> buffer{};
    int result = serialWrite(1, buffer.data(), -1, 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
}

TEST_F(SerialWriteTest, WriteInvalidHandleZero)
{
    const char *buffer = "test";
    int result = serialWrite(0, buffer, static_cast<int>(strlen(buffer)), 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialWriteTest, WriteInvalidHandleNegative)
{
    const char *buffer = "test";
    int result = serialWrite(-1, buffer, static_cast<int>(strlen(buffer)), 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialWriteTest, WriteInvalidHandleTooLarge)
{
    const char *buffer = "test";
    auto too_large = static_cast<int64_t>(std::numeric_limits<int>::max()) + 1;
    int result = serialWrite(too_large, buffer, static_cast<int>(strlen(buffer)), 100, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialWriteTest, WriteEmptyStringZeroSize)
{
    const char *empty = "";
    int result = serialWrite(1, empty, 0, 0, 0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kBufferError));
}

TEST_F(SerialWriteTest, WriteNoErrorCallback)
{
    std::array<char, 10> buffer{};
    int result = serialWrite(0, buffer.data(), 1, 0, 0, nullptr);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}
