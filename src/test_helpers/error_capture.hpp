#pragma once

#include <cpp_core/interface/serial_open.h>
#include <cpp_core/status_codes.h>

#include <string>

struct ErrorCapture
{
    int last_code = 0;
    std::string last_message;

    static void callback(int code, const char *message);

    static ErrorCapture *instance;
};
