// Simple integration test for cpp_unix_bindings
// ------------------------------------------------
// This executable opens the given serial port, sends a test
// string and verifies that the same string is echoed back
// by the micro-controller.
//
// ------------------------------------------------
// Arduino sketch to flash for the tests
// ------------------------------------------------
/*
  // --- BEGIN ARDUINO CODE ---
  void setup() {
      Serial.begin(115200);
      // Wait until the host opens the port (optional but handy)
      while (!Serial) {
          ;
      }
  }

  void loop() {
      if (Serial.available()) {
          char c = Serial.read();
          Serial.write(c);          // echo back
      }
  }
  // --- END ARDUINO CODE ---
*/
// ------------------------------------------------

#include "serial.h"

#include <array>
#include <chrono>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <thread>

namespace
{
const char* default_port = "/dev/ttyUSB0";
} // namespace

TEST(SerialEchoTest, EchoMessage)
{
    const std::string test_msg = "HELLO";

    intptr_t handle = serialOpen((void*)default_port, 115200, 8, 0, 0);
    ASSERT_NE(handle, 0) << "Failed to open port " << default_port;

    // Opening a serial connection toggles DTR on most Arduino boards, which
    // triggers a reset. Give the micro-controller a moment to reboot before we
    // start talking to it, otherwise the first bytes might be lost.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Send message
    int written = serialWrite(handle, (void*)test_msg.c_str(), static_cast<int>(test_msg.size()), 100, 1);
    ASSERT_EQ(written, static_cast<int>(test_msg.size())) << "Write failed";

    // Read echo
    std::array<char, 16> read_buffer{};
    int bytes_read = serialRead(handle, read_buffer.data(), static_cast<int>(test_msg.size()), 500, 1);
    ASSERT_EQ(bytes_read, static_cast<int>(test_msg.size())) << "Read failed (got " << bytes_read << ")";

    ASSERT_EQ(std::strncmp(read_buffer.data(), test_msg.c_str(), test_msg.size()), 0)
        << "Data mismatch: expected " << test_msg << ", got " << read_buffer.data();

    serialClose(handle);
}

TEST(SerialReadUntilTest, ReadUntilChar)
{
    const std::string test_msg = "WORLD\n"; // include terminator newline

    intptr_t handle = serialOpen((void*)default_port, 115200, 8, 0, 0);
    ASSERT_NE(handle, 0) << "Failed to open port " << default_port;

    // Give the board time to reset after opening the port.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Send message (write includes the terminator char)
    int written = serialWrite(handle, (void*)test_msg.c_str(), static_cast<int>(test_msg.size()), 100, 1);
    ASSERT_EQ(written, static_cast<int>(test_msg.size())) << "Write failed";

    // Read back until newline (inclusive)
    std::array<char, 32> buffer{0};
    char until = '\n';
    int read_bytes = serialReadUntil(handle, buffer.data(), static_cast<int>(buffer.size()), 500, 1, &until);
    ASSERT_EQ(read_bytes, static_cast<int>(test_msg.size())) << "serialReadUntil returned unexpected length";

    ASSERT_EQ(std::strncmp(buffer.data(), test_msg.c_str(), test_msg.size()), 0)
        << "Data mismatch: expected " << test_msg << ", got " << buffer.data();

    serialClose(handle);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    if (argc > 1)
    {
        default_port = argv[1];
    }
    return RUN_ALL_TESTS();
}
