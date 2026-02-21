// Integration test: pipe-based read/write round-trip is supported on Linux only.
// On Windows, serial read/write use COM port APIs and do not work with pipe handles.
// Use serial_arduino.test.cpp with a real or virtual COM port for integration testing.

#include <gtest/gtest.h>

TEST(SerialIntegrationTest, PipeRoundTripNotAvailableOnWindows)
{
    GTEST_SKIP() << "Pipe-based integration test is Linux-only. On Windows use a COM port and "
                    "serial_arduino.test.cpp (set SERIAL_TEST_PORT).";
}
