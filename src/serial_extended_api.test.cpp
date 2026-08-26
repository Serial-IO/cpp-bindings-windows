#include <cpp_core/interface/serial_abort_read.h>
#include <cpp_core/interface/serial_abort_write.h>
#include <cpp_core/interface/serial_in_bytes_total.h>
#include <cpp_core/interface/serial_list_ports.h>
#include <cpp_core/interface/serial_monitor_ports.h>
#include <cpp_core/interface/serial_out_bytes_total.h>
#include <cpp_core/interface/serial_read.h>
#include <cpp_core/interface/serial_read_until.h>
#include <cpp_core/interface/serial_read_until_sequence.h>
#include <cpp_core/interface/serial_set_error_callback.h>
#include <cpp_core/interface/serial_set_read_callback.h>
#include <cpp_core/interface/serial_set_write_callback.h>
#include <cpp_core/status_code.h>

#include <array>
#include <atomic>

#include <gtest/gtest.h>

namespace
{
std::atomic<int> g_last_error_code{0};
std::atomic<int> g_port_callback_count{0};

void globalErrorCallback(int code, const char * /*message*/)
{
    g_last_error_code.store(code, std::memory_order_relaxed);
}

void listPortsCallback(const char * /*port*/, const char * /*path*/, const char * /*manufacturer*/,
                       const char * /*serial_number*/, const char * /*pnp_id*/, const char * /*location_id*/,
                       const char * /*product_id*/, const char * /*vendor_id*/)
{
    g_port_callback_count.fetch_add(1, std::memory_order_relaxed);
}

constexpr auto kBufferError = static_cast<int>(cpp_core::StatusCode::Io::kBufferError);
constexpr auto kInvalidHandleError = static_cast<int>(cpp_core::StatusCode::Connection::kInvalidHandleError);
} // namespace

class SerialExtendedApiTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        g_last_error_code.store(0, std::memory_order_relaxed);
        g_port_callback_count.store(0, std::memory_order_relaxed);
        serialSetErrorCallback(nullptr);
        serialSetReadCallback(nullptr);
        serialSetWriteCallback(nullptr);
        ASSERT_EQ(serialMonitorPorts(nullptr, nullptr), 0);
    }

    void TearDown() override
    {
        serialSetErrorCallback(nullptr);
        serialSetReadCallback(nullptr);
        serialSetWriteCallback(nullptr);
        (void)serialMonitorPorts(nullptr, nullptr);
    }
};

TEST_F(SerialExtendedApiTest, GlobalErrorCallbackActsAsFallback)
{
    serialSetErrorCallback(globalErrorCallback);

    std::array<char, 4> buffer{};
    EXPECT_EQ(serialRead(-1, buffer.data(), static_cast<int>(buffer.size()), 10, 1, nullptr), kInvalidHandleError);
    EXPECT_EQ(g_last_error_code.load(std::memory_order_relaxed), kInvalidHandleError);
}

TEST_F(SerialExtendedApiTest, ReadHelpersValidateTerminators)
{
    std::array<char, 4> buffer{};
    EXPECT_EQ(serialReadUntil(1, buffer.data(), static_cast<int>(buffer.size()), 10, 1, nullptr, nullptr),
              kBufferError);
    EXPECT_EQ(serialReadUntilSequence(1, buffer.data(), static_cast<int>(buffer.size()), 10, 1, nullptr, nullptr),
              kBufferError);

    char empty_sequence[] = "";
    EXPECT_EQ(
        serialReadUntilSequence(1, buffer.data(), static_cast<int>(buffer.size()), 10, 1, empty_sequence, nullptr),
        kBufferError);
}

TEST_F(SerialExtendedApiTest, HandleBasedExtensionsRejectInvalidHandles)
{
    EXPECT_EQ(serialAbortRead(-1, nullptr), kInvalidHandleError);
    EXPECT_EQ(serialAbortWrite(-1, nullptr), kInvalidHandleError);
    EXPECT_EQ(serialInBytesTotal(-1, nullptr), kInvalidHandleError);
    EXPECT_EQ(serialOutBytesTotal(-1, nullptr), kInvalidHandleError);
}

TEST_F(SerialExtendedApiTest, ListPortsValidatesAndEnumerates)
{
    EXPECT_EQ(serialListPorts(nullptr, nullptr), kBufferError);

    const int result = serialListPorts(listPortsCallback, nullptr);
    EXPECT_GE(result, 0);
    EXPECT_EQ(result, g_port_callback_count.load(std::memory_order_relaxed));
}
