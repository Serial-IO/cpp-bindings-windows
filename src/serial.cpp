#include "serial.h"

#include "status_codes.h"
#include "version_config.h"

// Windows serial implementation
// -----------------------------------------------------------------------------
// NOTE: This file only supports the Windows API. All POSIX specific code has
// been removed.
// -----------------------------------------------------------------------------

#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <string_view>
#include <utility>
#include <windows.h>

// -----------------------------------------------------------------------------
// Global callback function pointers (default nullptr)
// -----------------------------------------------------------------------------
void (*on_error_callback)(int, const char*) = nullptr;
void (*on_read_callback)(int) = nullptr;
void (*on_write_callback)(int) = nullptr;

// -----------------------------------------------------------------------------
// Internal helpers & types
// -----------------------------------------------------------------------------
namespace
{

struct SerialPortHandle
{
    HANDLE handle{INVALID_HANDLE_VALUE};
    DCB original_dcb{}; // keep original settings so we can restore on close

    int64_t in_total{0};  // bytes received so far
    int64_t out_total{0}; // bytes transmitted so far

    std::atomic<bool> abort_read{false};
    std::atomic<bool> abort_write{false};
};

void invokeError(int code, const char* message)
{
    if (on_error_callback != nullptr)
    {
        on_error_callback(code, message);
    }
}

// Convert baudrate integer to constant directly (Windows API accepts int)
bool configurePort(HANDLE handle, int baudrate, int dataBits, int parity, int stopBits)
{
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (GetCommState(handle, &dcb) == 0)
    {
        return false;
    }

    // Store a copy of the original DCB? -> done outside

    dcb.BaudRate = static_cast<DWORD>(baudrate);
    dcb.ByteSize = static_cast<BYTE>(std::clamp(dataBits, 5, 8));

    // Parity
    switch (parity)
    {
    case 1: // even
        dcb.Parity = EVENPARITY;
        dcb.fParity = TRUE;
        break;
    case 2: // odd
        dcb.Parity = ODDPARITY;
        dcb.fParity = TRUE;
        break;
    default:
        dcb.Parity = NOPARITY;
        dcb.fParity = FALSE;
        break;
    }

    // Stop bits
    dcb.StopBits = (stopBits == 2) ? TWOSTOPBITS : ONESTOPBIT;

    // Disable hardware/software flow control
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX = FALSE;
    dcb.fInX = FALSE;

    return !(SetCommState(handle, &dcb) == 0);
}

void setPortTimeouts(HANDLE handle, int readTimeoutMs, int writeTimeoutMs)
{
    COMMTIMEOUTS timeouts{};

    // Read time-outs
    if (readTimeoutMs < 0)
    {
        // Infinite blocking
        timeouts.ReadIntervalTimeout = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.ReadTotalTimeoutMultiplier = 0;
    }
    else
    {
        timeouts.ReadIntervalTimeout = MAXDWORD; // return immediately if no bytes available
        timeouts.ReadTotalTimeoutConstant = static_cast<DWORD>(readTimeoutMs);
        timeouts.ReadTotalTimeoutMultiplier = 0;
    }

    // Write time-outs (simple constant component only)
    if (writeTimeoutMs >= 0)
    {
        timeouts.WriteTotalTimeoutConstant = static_cast<DWORD>(writeTimeoutMs);
        timeouts.WriteTotalTimeoutMultiplier = 0;
    }

    SetCommTimeouts(handle, &timeouts);
}

// Helper that adds the required "\\\\.\\" prefix for COM ports >= 10
std::string toWinComPath(std::string_view portView)
{
    std::string port(portView);
    // If the path already starts with \\.\, leave it
    if (port.starts_with(R"(\\.\)"))
    {
        return port;
    }
    // Prepend prefix so Windows can open COM10+
    return R"(\\.\)" + port;
}

} // namespace

// -----------------------------------------------------------------------------
// Public API implementation
// -----------------------------------------------------------------------------

intptr_t serialOpen(void* port, int baudrate, int dataBits, int parity, int stopBits)
{
    if (port == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialOpen: port parameter is null");
        return 0;
    }

    std::string_view port_name_view{static_cast<const char*>(port)};
    std::string win_port = toWinComPath(port_name_view);

    HANDLE raw_handle = CreateFileA(win_port.c_str(),
                                    GENERIC_READ | GENERIC_WRITE,
                                    0, // exclusive access
                                    nullptr,
                                    OPEN_EXISTING,
                                    FILE_ATTRIBUTE_NORMAL,
                                    nullptr);

    if (raw_handle == INVALID_HANDLE_VALUE)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialOpen: CreateFileA failed (INVALID_HANDLE_VALUE)");
        return 0;
    }

    // Save original DCB first
    DCB original{};
    original.DCBlength = sizeof(DCB);
    if (GetCommState(raw_handle, &original) == 0)
    {
        invokeError(std::to_underlying(StatusCodes::GET_STATE_ERROR), "serialOpen: GetCommState failed");
        CloseHandle(raw_handle);
        return 0;
    }

    // Configure DCB with requested settings
    if (!configurePort(raw_handle, baudrate, dataBits, parity, stopBits))
    {
        invokeError(std::to_underlying(StatusCodes::SET_STATE_ERROR), "serialOpen: configurePort failed");
        CloseHandle(raw_handle);
        return 0;
    }

    // Default timeouts – can be overridden per read/write call
    setPortTimeouts(raw_handle, 1000, 1000);

    auto* handle = new SerialPortHandle{};
    handle->handle = raw_handle;
    handle->original_dcb = original;

    return reinterpret_cast<intptr_t>(handle);
}

void serialClose(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        return;
    }

    // Restore original state
    SetCommState(handle->handle, &handle->original_dcb);
    FlushFileBuffers(handle->handle);
    CloseHandle(handle->handle);

    delete handle;
}

// --- Core IO helpers --------------------------------------------------------

static int readFromPort(SerialPortHandle* handle, void* buffer, int bufferSize, int timeoutMs)
{
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "readFromPort: handle pointer is null");
        return 0;
    }

    if (handle->abort_read.exchange(false))
    {
        return 0;
    }

    setPortTimeouts(handle->handle, timeoutMs, -1);

    DWORD bytes_read = 0;
    BOOL ok = ReadFile(handle->handle, buffer, static_cast<DWORD>(bufferSize), &bytes_read, nullptr);
    if (ok == 0)
    {
        invokeError(std::to_underlying(StatusCodes::READ_ERROR), "readFromPort: ReadFile failed");
        return 0;
    }

    if (bytes_read > 0)
    {
        handle->in_total += bytes_read;
        if (on_read_callback != nullptr)
        {
            on_read_callback(static_cast<int>(bytes_read));
        }
    }

    return static_cast<int>(bytes_read);
}

static int writeToPort(SerialPortHandle* handle, const void* buffer, int bufferSize, int timeoutMs)
{
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "writeToPort: handle pointer is null");
        return 0;
    }

    if (handle->abort_write.exchange(false))
    {
        return 0;
    }

    setPortTimeouts(handle->handle, -1, timeoutMs);

    DWORD bytes_written = 0;
    BOOL ok = WriteFile(handle->handle, buffer, static_cast<DWORD>(bufferSize), &bytes_written, nullptr);
    if (ok == 0)
    {
        invokeError(std::to_underlying(StatusCodes::WRITE_ERROR), "writeToPort: WriteFile failed");
        return 0;
    }

    if (bytes_written > 0)
    {
        handle->out_total += bytes_written;
        if (on_write_callback != nullptr)
        {
            on_write_callback(static_cast<int>(bytes_written));
        }
    }

    return static_cast<int>(bytes_written);
}

// --- Public IO wrappers -----------------------------------------------------

int serialRead(int64_t handlePtr, void* buffer, int bufferSize, int timeout, int /*multiplier*/)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialRead: handle pointer is null");
        return 0;
    }

    int bytes_read = readFromPort(handle, buffer, bufferSize, timeout);
    return bytes_read;
}

int serialWrite(int64_t handlePtr, const void* buffer, int bufferSize, int timeout, int /*multiplier*/)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    return writeToPort(handle, buffer, bufferSize, timeout);
}

// ---------------- Higher level helpers -------------------------------------

int serialReadUntil(int64_t handlePtr, void* buffer, int bufferSize, int timeout, int /*multiplier*/, void* untilCharPtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialReadUntil: handle pointer is null");
        return 0;
    }

    char until_char = *static_cast<char*>(untilCharPtr);
    int total = 0;
    auto* buf = static_cast<char*>(buffer);

    while (total < bufferSize)
    {
        int r = serialRead(handlePtr, buf + total, 1, timeout, 1);
        if (r <= 0)
        {
            break;
        }
        if (buf[total] == until_char)
        {
            total += 1;
            break;
        }
        total += r;
    }

    if (on_read_callback != nullptr)
    {
        on_read_callback(total);
    }
    return total;
}

int serialReadUntilSequence(int64_t handlePtr, void* buffer, int bufferSize, int timeout, void* sequencePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialReadUntilSequence: handle pointer is null");
        return 0;
    }

    const char* sequence = static_cast<const char*>(sequencePtr);
    int token_len = static_cast<int>(std::strlen(sequence));

    auto* buf = static_cast<char*>(buffer);
    int total = 0;
    int match_pos = 0;

    while (total < bufferSize)
    {
        int r = serialRead(handlePtr, buf + total, 1, timeout, 1);
        if (r <= 0)
        {
            break;
        }

        if (buf[total] == sequence[match_pos])
        {
            match_pos += 1;
            if (match_pos == token_len)
            {
                total += 1;
                break; // full token matched
            }
        }
        else
        {
            match_pos = 0; // reset match progress
        }
        total += r;
    }

    if (on_read_callback != nullptr)
    {
        on_read_callback(total);
    }
    return total;
}

// -----------------------------------------------------------------------------
// Buffer & abort helpers implementations
// -----------------------------------------------------------------------------

void serialClearBufferIn(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        return;
    }
    PurgeComm(handle->handle, PURGE_RXABORT | PURGE_RXCLEAR);
}

void serialClearBufferOut(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        return;
    }
    PurgeComm(handle->handle, PURGE_TXABORT | PURGE_TXCLEAR);
}

void serialAbortRead(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle != nullptr)
    {
        handle->abort_read = true;
    }
}

void serialAbortWrite(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle != nullptr)
    {
        handle->abort_write = true;
    }
}

// -----------------------------------------------------------------------------
// Callback registration
// -----------------------------------------------------------------------------

void serialOnRead(void (*callbackFn)(int bytes))
{
    on_read_callback = callbackFn;
}
void serialOnWrite(void (*callbackFn)(int bytes))
{
    on_write_callback = callbackFn;
}
void serialOnError(void (*callbackFn)(int code, const char* message))
{
    on_error_callback = callbackFn;
}

// -----------------------------------------------------------------------------
// Helper utilities (read line, read frame, statistics, etc.) copied/adapted
// from the original implementation as they remain platform-agnostic.
// -----------------------------------------------------------------------------

int serialReadLine(int64_t handlePtr, void* buffer, int bufferSize, int timeout)
{
    char newline = '\n';
    return serialReadUntil(handlePtr, buffer, bufferSize, timeout, 1, &newline);
}

int serialWriteLine(int64_t handlePtr, const void* buffer, int bufferSize, int timeout)
{
    // Write payload
    int bytes_written = serialWrite(handlePtr, buffer, bufferSize, timeout, 1);
    if (bytes_written != bufferSize)
    {
        return bytes_written;
    }

    // Append newline
    const char newline = '\n';
    int written_nl = serialWrite(handlePtr, &newline, 1, timeout, 1);
    return (written_nl == 1) ? (bytes_written + 1) : bytes_written;
}

int serialReadFrame(int64_t handlePtr, void* buffer, int bufferSize, int timeout, char startByte, char endByte)
{
    auto* buf = static_cast<char*>(buffer);
    int total = 0;

    // Wait for start byte
    char byte = 0;
    while (true)
    {
        int bytes_read = serialRead(handlePtr, &byte, 1, timeout, 1);
        if (bytes_read <= 0)
        {
            return 0; // timeout or error
        }
        if (byte == startByte)
        {
            buf[0] = byte;
            total = 1;
            break;
        }
    }

    // Read until end byte
    while (total < bufferSize)
    {
        int bytes_read = serialRead(handlePtr, &byte, 1, timeout, 1);
        if (bytes_read <= 0)
        {
            break;
        }
        buf[total] = byte;
        total += 1;
        if (byte == endByte)
        {
            break;
        }
    }

    return total;
}

// Statistics helpers
int64_t serialOutBytesTotal(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    return (handle != nullptr) ? handle->in_total : 0;
}

int64_t serialInBytesTotal(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    return (handle != nullptr) ? handle->out_total : 0;
}

int serialDrain(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialDrain: handle pointer is null");
        return 0;
    }

    if (FlushFileBuffers(handle->handle) == 0)
    {
        invokeError(std::to_underlying(StatusCodes::WRITE_ERROR), "serialDrain: FlushFileBuffers failed");
        return 0;
    }
    return 1;
}

// -----------------------------------------------------------------------------
// Buffer occupancy helpers (similar to PySerial in_waiting / out_waiting)
// -----------------------------------------------------------------------------

int serialInBytesWaiting(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialInWaiting: handle pointer is null");
        return 0;
    }

    COMSTAT status{};
    DWORD errors = 0;
    if (ClearCommError(handle->handle, &errors, &status) == 0)
    {
        invokeError(std::to_underlying(StatusCodes::GET_STATE_ERROR), "serialInWaiting: ClearCommError failed");
        return 0;
    }
    return static_cast<int>(status.cbInQue);
}

int serialOutBytesWaiting(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR), "serialOutWaiting: handle pointer is null");
        return 0;
    }

    COMSTAT status{};
    DWORD errors = 0;
    if (ClearCommError(handle->handle, &errors, &status) == 0)
    {
        invokeError(std::to_underlying(StatusCodes::GET_STATE_ERROR), "serialOutWaiting: ClearCommError failed");
        return 0;
    }
    return static_cast<int>(status.cbOutQue);
}
