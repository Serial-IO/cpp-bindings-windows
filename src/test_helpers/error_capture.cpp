#include "test_helpers/error_capture.hpp"

void ErrorCapture::callback(int code, const char *message)
{
    if (instance != nullptr)
    {
        instance->last_code = code;
        instance->last_message = message != nullptr ? message : "";
    }
}

ErrorCapture *ErrorCapture::instance = nullptr;
