#include <cpp_core/interface/serial_open.h>
#include <cpp_core/status_code.h>

#include <array>
#include <string>

#include "detail/windows.hpp"

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
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(nullptr, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Connection::kNotFoundError));
    EXPECT_NE(error_capture.last_message.find("nullptr"), std::string::npos);
}

TEST_F(SerialOpenTest, BaudrateTooLow)
{
    const cpp_core::SerialConfig config{100, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Configuration::kSetBaudrateError));
    EXPECT_NE(error_capture.last_message.find("baudrate"), std::string::npos);
}

TEST_F(SerialOpenTest, BaudrateTooLowBoundary)
{
    const cpp_core::SerialConfig config{299, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Configuration::kSetBaudrateError));
}

TEST_F(SerialOpenTest, BaudrateBoundaryValid)
{
    const cpp_core::SerialConfig config{300, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    // COM99999 does not exist, but should pass baudrate validation (kNotFoundError, not kSetStateError)
    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, DataBitsTooLow)
{
    const cpp_core::SerialConfig config{9600, static_cast<cpp_core::DataBits>(4), cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Configuration::kSetDataBitsError));
    EXPECT_NE(error_capture.last_message.find("data bits"), std::string::npos);
}

TEST_F(SerialOpenTest, DataBitsTooHigh)
{
    const cpp_core::SerialConfig config{9600, static_cast<cpp_core::DataBits>(9), cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Configuration::kSetDataBitsError));
}

TEST_F(SerialOpenTest, ValidDataBits5)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kFive, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits6)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kSix, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits7)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kSeven, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, ValidDataBits8)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, InvalidParity)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, static_cast<cpp_core::Parity>(5),
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_LT(result, 0);
}

TEST_F(SerialOpenTest, ValidParityNone)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, ValidParityEven)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kEven,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, ValidParityOdd)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kOdd,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, InvalidStopBits)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        static_cast<cpp_core::StopBits>(3), cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_LT(result, 0);
}

TEST_F(SerialOpenTest, ValidStopBits0)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, RejectsLegacyStopBits1)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        static_cast<cpp_core::StopBits>(1), cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Configuration::kSetStopBitsError));
}

TEST_F(SerialOpenTest, ValidStopBits2)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kTwo, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError));
}

TEST_F(SerialOpenTest, NonExistentPort)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Connection::kNotFoundError));
}

TEST_F(SerialOpenTest, VariousBaudrates)
{
    const std::array<int, 11> baudrates = {300, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400, 460800};

    for (int baudrate : baudrates)
    {
        const cpp_core::SerialConfig config{baudrate, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                            cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
        intptr_t result = serialOpen(kNonExistentPort, &config, error_callback);
        EXPECT_NE(result, static_cast<intptr_t>(cpp_core::StatusCode::Control::kSetStateError))
            << "Baudrate " << baudrate << " should be valid";
    }
}

TEST_F(SerialOpenTest, NoErrorCallbackNullPort)
{
    const cpp_core::SerialConfig config{9600, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                        cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
    intptr_t result = serialOpen(nullptr, &config, nullptr);

    EXPECT_EQ(result, static_cast<intptr_t>(cpp_core::StatusCode::Connection::kNotFoundError));
}
