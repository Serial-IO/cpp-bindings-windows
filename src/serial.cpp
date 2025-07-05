#include "serial.h"
#include "status_codes.h"

// Windows serial implementation
// -----------------------------------------------------------------------------
// NOTE: This file only supports the Windows API. All POSIX specific code has
// been removed.
// -----------------------------------------------------------------------------

#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

// -----------------------------------------------------------------------------
// Global callback function pointers (default nullptr)
// -----------------------------------------------------------------------------
void (*error_callback)(int) = nullptr;
void (*read_callback)(int) = nullptr;
void (*write_callback)(int) = nullptr;

// -----------------------------------------------------------------------------
// Internal helpers & types
// -----------------------------------------------------------------------------
namespace
{

struct SerialPortHandle
{
    HANDLE handle{INVALID_HANDLE_VALUE};
    DCB    original_dcb{}; // keep original settings so we can restore on close

    int64_t rx_total{0}; // bytes received so far
    int64_t tx_total{0}; // bytes transmitted so far

    bool has_peek{false};
    char peek_char{0};

    std::atomic<bool> abort_read{false};
    std::atomic<bool> abort_write{false};
};

inline void invokeError(int code)
{
    if (error_callback != nullptr)
    {
        error_callback(code);
    }
}

// Convert baudrate integer to constant directly (Windows API accepts int)
inline bool configurePort(HANDLE h, int baudrate, int dataBits, int parity, int stopBits)
{
    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &dcb))
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
        dcb.Parity   = EVENPARITY;
        dcb.fParity  = TRUE;
        break;
    case 2: // odd
        dcb.Parity   = ODDPARITY;
        dcb.fParity  = TRUE;
        break;
    default:
        dcb.Parity   = NOPARITY;
        dcb.fParity  = FALSE;
        break;
    }

    // Stop bits
    dcb.StopBits = (stopBits == 2) ? TWOSTOPBITS : ONESTOPBIT;

    // Disable hardware/software flow control
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fOutX        = FALSE;
    dcb.fInX         = FALSE;

    return !!SetCommState(h, &dcb);
}

inline void setPortTimeouts(HANDLE h, int readTimeoutMs, int writeTimeoutMs)
{
    COMMTIMEOUTS timeouts{};

    // Read time-outs
    if (readTimeoutMs < 0)
    {
        // Infinite blocking
        timeouts.ReadIntervalTimeout         = 0;
        timeouts.ReadTotalTimeoutConstant     = 0;
        timeouts.ReadTotalTimeoutMultiplier   = 0;
    }
    else
    {
        timeouts.ReadIntervalTimeout         = MAXDWORD; // return immediately if no bytes available
        timeouts.ReadTotalTimeoutConstant     = static_cast<DWORD>(readTimeoutMs);
        timeouts.ReadTotalTimeoutMultiplier   = 0;
    }

    // Write time-outs (simple constant component only)
    if (writeTimeoutMs >= 0)
    {
        timeouts.WriteTotalTimeoutConstant   = static_cast<DWORD>(writeTimeoutMs);
        timeouts.WriteTotalTimeoutMultiplier = 0;
    }

    SetCommTimeouts(h, &timeouts);
}

// Helper that adds the required "\\\\.\\" prefix for COM ports >= 10
inline std::string toWinComPath(std::string_view port)
{
    std::string p(port);
    // If the path already starts with \\.\, leave it
    if (p.rfind("\\\\.\\", 0) == 0)
    {
        return p;
    }
    // Prepend prefix so Windows can open COM10+
    return "\\\\.\\" + p;
}

} // namespace

// -----------------------------------------------------------------------------
// Public API implementation
// -----------------------------------------------------------------------------

intptr_t serialOpen(void* port, int baudrate, int dataBits, int parity, int stopBits)
{
    if (port == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    std::string_view portNameView{static_cast<const char*>(port)};
    std::string      winPort     = toWinComPath(portNameView);

    HANDLE h = CreateFileA(winPort.c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0,               // exclusive access
                           nullptr,
                           OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL,
                           nullptr);

    if (h == INVALID_HANDLE_VALUE)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    // Save original DCB first
    DCB original{};
    original.DCBlength = sizeof(DCB);
    if (!GetCommState(h, &original))
    {
        invokeError(std::to_underlying(StatusCodes::GET_STATE_ERROR));
        CloseHandle(h);
        return 0;
    }

    // Configure DCB with requested settings
    if (!configurePort(h, baudrate, dataBits, parity, stopBits))
    {
        invokeError(std::to_underlying(StatusCodes::SET_STATE_ERROR));
        CloseHandle(h);
        return 0;
    }

    // Default timeouts – can be overridden per read/write call
    setPortTimeouts(h, 1000, 1000);

    auto* handle = new SerialPortHandle{};
    handle->handle        = h;
    handle->original_dcb  = original;

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
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    if (handle->abort_read.exchange(false))
    {
        return 0;
    }

    setPortTimeouts(handle->handle, timeoutMs, -1);

    DWORD bytesRead = 0;
    BOOL  ok        = ReadFile(handle->handle, buffer, static_cast<DWORD>(bufferSize), &bytesRead, nullptr);
    if (!ok)
    {
        invokeError(std::to_underlying(StatusCodes::READ_ERROR));
        return 0;
    }

    if (bytesRead > 0)
    {
        handle->rx_total += bytesRead;
        if (read_callback != nullptr)
        {
            read_callback(static_cast<int>(bytesRead));
        }
    }

    return static_cast<int>(bytesRead);
}

static int writeToPort(SerialPortHandle* handle, const void* buffer, int bufferSize, int timeoutMs)
{
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    if (handle->abort_write.exchange(false))
    {
        return 0;
    }

    setPortTimeouts(handle->handle, -1, timeoutMs);

    DWORD bytesWritten = 0;
    BOOL  ok           = WriteFile(handle->handle, buffer, static_cast<DWORD>(bufferSize), &bytesWritten, nullptr);
    if (!ok)
    {
        invokeError(std::to_underlying(StatusCodes::WRITE_ERROR));
        return 0;
    }

    if (bytesWritten > 0)
    {
        handle->tx_total += bytesWritten;
        if (write_callback != nullptr)
        {
            write_callback(static_cast<int>(bytesWritten));
        }
    }

    return static_cast<int>(bytesWritten);
}

// --- Public IO wrappers -----------------------------------------------------

int serialRead(int64_t handlePtr, void* buffer, int bufferSize, int timeout, int /*multiplier*/)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    // First deliver byte from internal peek buffer if present
    int totalCopied = 0;
    if (handle->has_peek && bufferSize > 0)
    {
        static_cast<char*>(buffer)[0] = handle->peek_char;
        handle->has_peek              = false;
        handle->rx_total += 1;
        totalCopied       = 1;

        buffer     = static_cast<char*>(buffer) + 1;
        bufferSize -= 1;

        if (bufferSize == 0)
        {
            if (read_callback != nullptr)
            {
                read_callback(totalCopied);
            }
            return totalCopied;
        }
    }

    int bytesRead = readFromPort(handle, buffer, bufferSize, timeout);
    return totalCopied + bytesRead;
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
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    char untilChar = *static_cast<char*>(untilCharPtr);
    int  total     = 0;
    auto* buf      = static_cast<char*>(buffer);

    while (total < bufferSize)
    {
        int r = serialRead(handlePtr, buf + total, 1, timeout, 1);
        if (r <= 0)
        {
            break;
        }
        if (buf[total] == untilChar)
        {
            total += 1;
            break;
        }
        total += r;
    }

    if (read_callback != nullptr)
    {
        read_callback(total);
    }
    return total;
}

// List available COM ports using QueryDosDevice
int serialGetPortsInfo(void* buffer, int bufferSize, void* separatorPtr)
{
    const std::string_view sep{static_cast<const char*>(separatorPtr)};
    std::string            result;

    constexpr int maxPorts = 256;
    char       pathBuf[256];

    for (int i = 1; i <= maxPorts; ++i)
    {
        std::string port = "COM" + std::to_string(i);
        if (QueryDosDeviceA(port.c_str(), pathBuf, sizeof(pathBuf)))
        {
            result += port;
            result += sep;
        }
    }

    if (!result.empty())
    {
        // Remove trailing separator
        result.erase(result.size() - sep.size());
    }

    if (static_cast<int>(result.size()) + 1 > bufferSize)
    {
        invokeError(std::to_underlying(StatusCodes::BUFFER_ERROR));
        return 0;
    }

    std::memcpy(buffer, result.c_str(), result.size() + 1);
    return result.empty() ? 0 : 1;
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
    handle->has_peek = false;
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

void serialOnError(void (*func)(int code)) { error_callback = func; }
void serialOnRead(void (*func)(int bytes)) { read_callback   = func; }
void serialOnWrite(void (*func)(int bytes)) { write_callback = func; }

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
    int bytesWritten = serialWrite(handlePtr, buffer, bufferSize, timeout, 1);
    if (bytesWritten != bufferSize)
    {
        return bytesWritten;
    }

    // Append newline
    const char newline = '\n';
    int writtenNl      = serialWrite(handlePtr, &newline, 1, timeout, 1);
    return (writtenNl == 1) ? (bytesWritten + 1) : bytesWritten;
}

int serialReadUntilToken(int64_t handlePtr, void* buffer, int bufferSize, int timeout, void* tokenPtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    const char* token = static_cast<const char*>(tokenPtr);
    int         tokenLen = static_cast<int>(std::strlen(token));

    auto* buf = static_cast<char*>(buffer);
    int   total = 0;
    int   matchPos = 0;

    while (total < bufferSize)
    {
        int r = serialRead(handlePtr, buf + total, 1, timeout, 1);
        if (r <= 0)
        {
            break;
        }

        if (buf[total] == token[matchPos])
        {
            matchPos += 1;
            if (matchPos == tokenLen)
            {
                total += 1;
                break; // full token matched
            }
        }
        else
        {
            matchPos = 0; // reset match progress
        }
        total += r;
    }

    if (read_callback != nullptr)
    {
        read_callback(total);
    }
    return total;
}

int serialReadFrame(int64_t handlePtr, void* buffer, int bufferSize, int timeout, char startByte, char endByte)
{
    auto* buf   = static_cast<char*>(buffer);
    int   total = 0;

    // Wait for start byte
    char byte = 0;
    while (true)
    {
        int r = serialRead(handlePtr, &byte, 1, timeout, 1);
        if (r <= 0)
        {
            return 0; // timeout or error
        }
        if (byte == startByte)
        {
            buf[0] = byte;
            total  = 1;
            break;
        }
    }

    // Read until end byte
    while (total < bufferSize)
    {
        int r = serialRead(handlePtr, &byte, 1, timeout, 1);
        if (r <= 0)
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
int64_t serialGetRxBytes(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    return (handle != nullptr) ? handle->rx_total : 0;
}

int64_t serialGetTxBytes(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    return (handle != nullptr) ? handle->tx_total : 0;
}

int serialPeek(int64_t handlePtr, void* outByte, int timeout)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr || outByte == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    if (handle->has_peek)
    {
        *static_cast<char*>(outByte) = handle->peek_char;
        return 1;
    }

    int r = serialRead(handlePtr, &handle->peek_char, 1, timeout, 1);
    if (r == 1)
    {
        handle->has_peek           = true;
        *static_cast<char*>(outByte) = handle->peek_char;
    }
    return r;
}

int serialDrain(int64_t handlePtr)
{
    auto* handle = reinterpret_cast<SerialPortHandle*>(handlePtr);
    if (handle == nullptr)
    {
        invokeError(std::to_underlying(StatusCodes::INVALID_HANDLE_ERROR));
        return 0;
    }

    if (!FlushFileBuffers(handle->handle))
    {
        invokeError(std::to_underlying(StatusCodes::WRITE_ERROR));
        return 0;
    }
    return 1;
} 
