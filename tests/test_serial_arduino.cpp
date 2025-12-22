#include <cpp_core/interface/serial_close.h>
#include <cpp_core/interface/serial_open.h>
#include <cpp_core/interface/serial_read.h>
#include <cpp_core/interface/serial_write.h>
#include <cpp_core/status_codes.h>
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
auto widenUtf8(const char *s) -> std::wstring
{
    if (s == nullptr || s[0] == '\0')
    {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (needed <= 0)
    {
        return {};
    }

    std::wstring out;
    out.resize(static_cast<size_t>(needed));
    const int written = MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), needed);
    if (written <= 0)
    {
        return {};
    }

    // drop trailing null terminator for convenience
    if (!out.empty() && out.back() == L'\0')
    {
        out.pop_back();
    }
    return out;
}

auto readExact(intptr_t handle, char *dst, int want_bytes, int total_timeout_ms) -> int
{
    if (dst == nullptr || want_bytes <= 0)
    {
        return 0;
    }

    const ULONGLONG start = GetTickCount64();
    int total = 0;
    while (total < want_bytes)
    {
        const ULONGLONG now = GetTickCount64();
        const int elapsed = static_cast<int>(now - start);
        if (elapsed >= total_timeout_ms)
        {
            break;
        }

        // Read remaining bytes with a small per-call timeout to make progress.
        const int remaining = want_bytes - total;
        const int chunk = serialRead(handle, dst + total, remaining, 200, 1, nullptr);
        if (chunk < 0)
        {
            return chunk;
        }
        if (chunk == 0)
        {
            Sleep(10);
            continue;
        }
        total += chunk;
    }

    return total;
}
} // namespace

class SerialArduinoTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const char *env_port = std::getenv("SERIAL_TEST_PORT");
        std::wstring port_w = widenUtf8(env_port);
        if (port_w.empty())
        {
            port_w = L"COM5";
        }

        handle_ = serialOpen(const_cast<void *>(static_cast<const void *>(port_w.c_str())), 115200, 8, 0, 0, nullptr);
        ASSERT_GT(handle_, 0) << "Could not open serial port. Set SERIAL_TEST_PORT (e.g. COM5) or provide a working "
                                 "virtual/real serial device.";

        // Arduino resets on open; wait a bit.
        Sleep(2000);
        port_w_ = std::move(port_w);
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
    std::wstring port_w_;
};

TEST_F(SerialArduinoTest, OpenClose)
{
    EXPECT_GT(handle_, 0) << "serialOpen should return a positive handle";
}

TEST_F(SerialArduinoTest, WriteReadEcho)
{
    const char *test_message = "Hello Arduino!\n";
    const int message_len = static_cast<int>(strlen(test_message));

    const int written = serialWrite(handle_, test_message, message_len, 1000, 1, nullptr);
    EXPECT_EQ(written, message_len) << "Should write all bytes. Written: " << written << ", Expected: " << message_len;

    Sleep(500);

    char read_buffer[256] = {0};
    const int read_bytes = readExact(handle_, read_buffer, message_len, 3000);

    EXPECT_GT(read_bytes, 0) << "Should read at least some bytes";
    EXPECT_EQ(read_bytes, message_len) << "Should read exactly the echoed message length";
    EXPECT_EQ(std::string_view(read_buffer, static_cast<size_t>(message_len)),
              std::string_view(test_message, static_cast<size_t>(message_len)))
        << "Echoed content should match what was sent";
}

TEST_F(SerialArduinoTest, MultipleEchoCycles)
{
    const char *messages[] = {"Test1\n", "Test2\n", "Test3\n"};
    const int num_messages = 3;

    for (int i = 0; i < num_messages; ++i)
    {
        const int msg_len = static_cast<int>(strlen(messages[i]));

        const int written = serialWrite(handle_, messages[i], msg_len, 1000, 1, nullptr);
        EXPECT_EQ(written, msg_len) << "Cycle " << i << ": write failed";

        Sleep(500);

        char read_buffer[256] = {0};
        const int read_bytes = readExact(handle_, read_buffer, msg_len, 3000);
        EXPECT_EQ(read_bytes, msg_len) << "Cycle " << i << ": read size mismatch";
        EXPECT_EQ(std::string_view(read_buffer, static_cast<size_t>(msg_len)),
                  std::string_view(messages[i], static_cast<size_t>(msg_len)))
            << "Cycle " << i << ": echo content mismatch";
    }
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
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError))
        << "Should return error for invalid handle";
}

TEST(SerialInvalidHandleTest, InvalidHandleWrite)
{
    const char *data = "test";
    const int result = serialWrite(-1, data, 4, 1000, 1, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kInvalidHandleError))
        << "Should return error for invalid handle";
}

TEST(SerialInvalidHandleTest, InvalidHandleClose)
{
    const int result = serialClose(-1, nullptr);
    EXPECT_EQ(result, static_cast<int>(cpp_core::StatusCodes::kSuccess));
}
