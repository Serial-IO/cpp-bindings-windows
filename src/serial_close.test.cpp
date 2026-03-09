#include <cpp_core/interface/serial_close.h>
#include <cpp_core/status_codes.h>

#include <limits>

#include <gtest/gtest.h>

#include "test_helpers/error_capture.hpp"

class SerialCloseTest : public ::testing::Test
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

TEST_F(SerialCloseTest, CloseInvalidHandleZero)
{
    int result = serialClose(0, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kSuccess));
}

TEST_F(SerialCloseTest, CloseInvalidHandleNegative)
{
    int result = serialClose(-1, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kSuccess));
}

TEST_F(SerialCloseTest, CloseInvalidHandleNegativeLarge)
{
    int result = serialClose(-12345, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kSuccess));
}

TEST_F(SerialCloseTest, CloseInvalidHandleTooLarge)
{
    auto too_large_handle = static_cast<int64_t>(std::numeric_limits<int>::max()) + 1;
    int result = serialClose(too_large_handle, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialCloseTest, CloseInvalidHandleIntMaxBoundary)
{
    auto handle = static_cast<int64_t>(std::numeric_limits<int>::max());
    int result = serialClose(handle, error_callback);

    EXPECT_NE(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError));
}

TEST_F(SerialCloseTest, CloseNoErrorCallback)
{
    int result = serialClose(0, nullptr);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kSuccess));
}

TEST_F(SerialCloseTest, CloseInvalidHandle)
{
    // Closing a value that is not a valid HANDLE
    int result = serialClose(9999, error_callback);

    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kCloseHandleError));
}
