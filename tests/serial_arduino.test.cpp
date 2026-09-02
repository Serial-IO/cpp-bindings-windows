#include <cpp_core/interface/serial_clear_buffer_in.h>
#include <cpp_core/interface/serial_close.h>
#include <cpp_core/interface/serial_open.h>
#include <cpp_core/interface/serial_read.h>
#include <cpp_core/interface/serial_read_until.h>
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
#include <vector>

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
        const int bytes_read =
            serialRead(handle, destination + total_bytes_read, remaining_byte_count, 200, 1, nullptr);
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

        handle_ = serialOpen(const_cast<void *>(static_cast<const void *>(port)), 115200, 8, 0, 0, nullptr);
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

    const int bytes_written = serialWrite(handle_, test_message, message_length, 1000, 1, nullptr);
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

        const int bytes_written = serialWrite(handle_, messages[message_index], message_length, 1000, 1, nullptr);
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

TEST_F(SerialArduinoTest, ReadUntilHandlesLongPayload)
{
    ASSERT_EQ(serialClearBufferIn(handle_, nullptr), 0);

    std::string message(1000, 'A');
    message.push_back('\n');
    ASSERT_EQ(serialWrite(handle_, message.data(), static_cast<int>(message.size()), 3000, 1, nullptr),
              static_cast<int>(message.size()));

    std::vector<char> buffer(message.size());
    char newline = '\n';
    const int read_bytes =
        serialReadUntil(handle_, buffer.data(), static_cast<int>(buffer.size()), 3000, 1, &newline, nullptr);

    ASSERT_EQ(read_bytes, static_cast<int>(message.size()));
    EXPECT_EQ(std::string_view(buffer.data(), static_cast<std::size_t>(read_bytes)), message);
}

TEST_F(SerialArduinoTest, ReadUntilSequenceHandlesLongPayload)
{
    ASSERT_EQ(serialClearBufferIn(handle_, nullptr), 0);

    std::string message(1000, 'A');
    message += "\r\n";
    ASSERT_EQ(serialWrite(handle_, message.data(), static_cast<int>(message.size()), 3000, 1, nullptr),
              static_cast<int>(message.size()));

    std::vector<char> buffer(message.size());
    char sequence[] = "\r\n";
    const int read_bytes =
        serialReadUntilSequence(handle_, buffer.data(), static_cast<int>(buffer.size()), 3000, 1, sequence, nullptr);

    ASSERT_EQ(read_bytes, static_cast<int>(message.size()));
    EXPECT_EQ(std::string_view(buffer.data(), static_cast<std::size_t>(read_bytes)), message);
}

TEST_F(SerialArduinoTest, ReadTimeout)
{
    char buffer[256];
    const int read_bytes = serialRead(handle_, buffer, static_cast<int>(sizeof(buffer)), 100, 1, nullptr);
    EXPECT_GE(read_bytes, 0) << "Timeout should return 0, not error";
}

TEST(SerialInvalidHandleTest, InvalidHandleRead)
{
    char buffer[256];
    const int result = serialRead(-1, buffer, static_cast<int>(sizeof(buffer)), 1000, 1, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError))
        << "Should return error for invalid handle";
}

TEST(SerialInvalidHandleTest, InvalidHandleWrite)
{
    const char *data = "test";
    const int result = serialWrite(-1, data, 4, 1000, 1, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError))
        << "Should return error for invalid handle";
}

TEST(SerialInvalidHandleTest, InvalidHandleClose)
{
    const int result = serialClose(-1, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCode::kSuccess));
}
