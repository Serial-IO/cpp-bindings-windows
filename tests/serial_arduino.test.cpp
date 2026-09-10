#include <cpp_core/interface/serial_close.h>
#include <cpp_core/interface/serial_open.h>
#include <cpp_core/interface/serial_read.h>
#include <cpp_core/interface/serial_read_until_sequence.h>
#include <cpp_core/interface/serial_write.h>
#include <cpp_core/status_code.h>
#include <gtest/gtest.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>

namespace
{
auto readExact(intptr_t handle, char *destination, int requested_byte_count, int total_timeout_ms) -> int
{
    if (destination == nullptr || requested_byte_count <= 0)
    {
        return 0;
    }

    const ULONGLONG start = GetTickCount64();
    int total_bytes_read = 0;
    while (total_bytes_read < requested_byte_count)
    {
        const ULONGLONG now = GetTickCount64();
        const int elapsed_milliseconds = static_cast<int>(now - start);
        if (elapsed_milliseconds >= total_timeout_ms)
        {
            break;
        }

        // Read remaining bytes with a small per-call timeout to make progress.
        const int remaining_byte_count = requested_byte_count - total_bytes_read;
        const cpp_core::SerialTimeoutConfig timeout_config0{200, 1};
        const int bytes_read = serialRead(handle, reinterpret_cast<std::uint8_t *>(destination + total_bytes_read),
                                          remaining_byte_count, &timeout_config0, nullptr);
        if (bytes_read < 0)
        {
            return bytes_read;
        }
        if (bytes_read == 0)
        {
            Sleep(10);
            continue;
        }
        total_bytes_read += bytes_read;
    }

    return total_bytes_read;
}
} // namespace

class SerialArduinoTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const char *environment_port = std::getenv("SERIAL_TEST_PORT");
        const char *port = (environment_port != nullptr && environment_port[0] != '\0') ? environment_port : "COM5";

        const cpp_core::SerialConfig config1{115200, cpp_core::DataBits::kEight, cpp_core::Parity::kNone,
                                             cpp_core::StopBits::kOne, cpp_core::FlowControl::kNone};
        handle_ = serialOpen(port, &config1, nullptr);
        if (handle_ <= 0)
        {
            GTEST_SKIP() << "Could not open serial port '" << (environment_port ? environment_port : "COM5")
                         << "'. Set SERIAL_TEST_PORT (e.g. COM5) or connect Arduino.";
        }

        // Arduino resets on open; wait a bit.
        Sleep(2000);
    }

    void TearDown() override
    {
        if (handle_ > 0)
        {
            serialClose(handle_, nullptr);
            handle_ = 0;
        }
    }

    intptr_t handle_ = 0;
};

TEST_F(SerialArduinoTest, OpenClose)
{
    EXPECT_GT(handle_, 0) << "serialOpen should return a positive handle";
}

TEST_F(SerialArduinoTest, WriteReadEcho)
{
    const char *test_message = "Hello Arduino!\n";
    const int message_length = static_cast<int>(strlen(test_message));

    const cpp_core::SerialTimeoutConfig timeout_config2{1000, 1};
    const int bytes_written = serialWrite(handle_, reinterpret_cast<const std::uint8_t *>(test_message), message_length,
                                          &timeout_config2, nullptr);
    EXPECT_EQ(bytes_written, message_length)
        << "Should write all bytes. Written: " << bytes_written << ", Expected: " << message_length;

    Sleep(500);

    char read_buffer[256] = {0};
    const int read_bytes = readExact(handle_, read_buffer, message_length, 3000);

    EXPECT_GT(read_bytes, 0) << "Should read at least some bytes";
    EXPECT_EQ(read_bytes, message_length) << "Should read exactly the echoed message length";
    EXPECT_EQ(std::string_view(read_buffer, static_cast<size_t>(message_length)),
              std::string_view(test_message, static_cast<size_t>(message_length)))
        << "Echoed content should match what was sent";
}

TEST_F(SerialArduinoTest, MultipleEchoCycles)
{
    const char *messages[] = {"Test1\n", "Test2\n", "Test3\n"};
    const int message_count = 3;

    for (int message_index = 0; message_index < message_count; ++message_index)
    {
        const int message_length = static_cast<int>(strlen(messages[message_index]));

        const cpp_core::SerialTimeoutConfig timeout_config3{1000, 1};
        const int bytes_written = serialWrite(handle_, reinterpret_cast<const std::uint8_t *>(messages[message_index]),
                                              message_length, &timeout_config3, nullptr);
        EXPECT_EQ(bytes_written, message_length) << "Cycle " << message_index << ": write failed";

        Sleep(500);

        char read_buffer[256] = {0};
        const int read_bytes = readExact(handle_, read_buffer, message_length, 3000);
        EXPECT_EQ(read_bytes, message_length) << "Cycle " << message_index << ": read size mismatch";
        EXPECT_EQ(std::string_view(read_buffer, static_cast<size_t>(message_length)),
                  std::string_view(messages[message_index], static_cast<size_t>(message_length)))
            << "Cycle " << message_index << ": echo content mismatch";
    }
}

TEST_F(SerialArduinoTest, ReadTimeout)
{
    char buffer[256];
    const cpp_core::SerialTimeoutConfig timeout_config4{100, 1};
    const int read_bytes = serialRead(handle_, reinterpret_cast<std::uint8_t *>(buffer),
                                      static_cast<int>(sizeof(buffer)), &timeout_config4, nullptr);
    EXPECT_GE(read_bytes, 0) << "Timeout should return 0, not error";
}

TEST(SerialInvalidHandleTest, InvalidHandleRead)
{
    char buffer[256];
    const cpp_core::SerialTimeoutConfig timeout_config5{1000, 1};
    const int result = serialRead(-1, reinterpret_cast<std::uint8_t *>(buffer), static_cast<int>(sizeof(buffer)),
                                  &timeout_config5, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError))
        << "Should return error for invalid handle";
}

TEST(SerialInvalidHandleTest, InvalidHandleWrite)
{
    const char *data = "test";
    const cpp_core::SerialTimeoutConfig timeout_config6{1000, 1};
    const int result = serialWrite(-1, reinterpret_cast<const std::uint8_t *>(data), 4, &timeout_config6, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError))
        << "Should return error for invalid handle";
}

TEST(SerialInvalidHandleTest, InvalidHandleClose)
{
    const int result = serialClose(-1, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::kSuccess));
}

TEST_F(SerialArduinoTest, ReadUntilBinarySequenceLeavesTrailingData)
{
    constexpr std::uint8_t payload[] = {'a', 0, 'b', 'x', 'y'};
    constexpr std::uint8_t terminator[] = {0, 'b'};
    constexpr auto timeout = cpp_core::SerialTimeoutConfig::make<1000, 1>();
    ASSERT_EQ(serialWrite(handle_, payload, 5, &timeout), 5);
    std::uint8_t buffer[16]{};
    ASSERT_EQ(serialReadUntilSequence(handle_, buffer, 16, &timeout, terminator, 2), 3);
    EXPECT_EQ(std::memcmp(buffer, payload, 3), 0);
    ASSERT_EQ(readExact(handle_, reinterpret_cast<char *>(buffer), 2, 3000), 2);
    EXPECT_EQ(buffer[0], 'x');
    EXPECT_EQ(buffer[1], 'y');
}
