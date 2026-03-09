#include <cpp_core/interface/serial_open.h>
#include <cpp_core/status_codes.h>

#include <array>
#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <gtest/gtest.h>

#include "test_helpers/error_capture.hpp"

namespace
{
const char *kNonExistentPort = "COM99999";
}

class SerialOpenTest : public ::testing::Test
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

TEST_F(SerialOpenTest, NullPortParameter)
{
    intptr_t result = serialOpen(nullptr, 9600, 8, 0, 1, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kNotFoundError));
    EXPECT_NE(error_capture.last_message.find("nullptr"), std::string::npos);
}

TEST_F(SerialOpenTest, BaudrateTooLow)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 100, 8, 0, 1,
                                  error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
    EXPECT_NE(error_capture.last_message.find("baudrate"), std::string::npos);
}

TEST_F(SerialOpenTest, BaudrateTooLowBoundary)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 299, 8, 0, 1,
                                  error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, BaudrateBoundaryValid)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 300, 8, 0, 1,
                                  error_callback);

    // COM99999 does not exist, but should pass baudrate validation (kNotFoundError, not kSetStateError)
    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, DataBitsTooLow)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 4, 0, 1,
                                  error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
    EXPECT_NE(error_capture.last_message.find("data bits"), std::string::npos);
}

TEST_F(SerialOpenTest, DataBitsTooHigh)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 9, 0, 1,
                                  error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits5)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 5, 0, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits6)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 6, 0, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits7)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 7, 0, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits8)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, InvalidParity)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 5, 1,
                                  error_callback);

    EXPECT_LT(result, 0);
}

TEST_F(SerialOpenTest, ValidParityNone)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidParityEven)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 1, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidParityOdd)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 2, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, InvalidStopBits)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 3,
                                  error_callback);

    EXPECT_LT(result, 0);
}

TEST_F(SerialOpenTest, ValidStopBits0)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 0,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidStopBits1)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 1,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, ValidStopBits2)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 2,
                                  error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError));
}

TEST_F(SerialOpenTest, NonExistentPort)
{
    intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), 9600, 8, 0, 1,
                                  error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kNotFoundError));
}

TEST_F(SerialOpenTest, VariousBaudrates)
{
    const std::array<int, 11> baudrates = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800};

    for (int baudrate : baudrates)
    {
        intptr_t result = serialOpen(const_cast<void *>(static_cast<const void *>(kNonExistentPort)), baudrate, 8, 0,
                                     1, error_callback);
        EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCodes::kSetStateError))
            << "Baudrate " << baudrate << " should be valid";
    }
}

TEST_F(SerialOpenTest, NoErrorCallbackNullPort)
{
    intptr_t result = serialOpen(nullptr, 9600, 8, 0, 1, nullptr);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCodes::kNotFoundError));
}
