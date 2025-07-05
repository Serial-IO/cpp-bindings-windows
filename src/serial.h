#pragma once
#include "version_config.h"

#include <cstdint>

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef cpp_windows_bindings_EXPORTS
#define MODULE_API __declspec(dllexport)
#else
#define MODULE_API __declspec(dllimport)
#endif
#else
#define MODULE_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    MODULE_API void getVersion(Version out);

    // Basic serial API
    MODULE_API intptr_t
    serialOpen(void* port, int baudrate, int dataBits, int parity /*0-none,1-even,2-odd*/ = 0, int stopBits /*0-1bit,2-2bit*/ = 0);

    MODULE_API void serialClose(int64_t handle);

    MODULE_API int serialRead(int64_t handle, void* buffer, int bufferSize, int timeout /*ms*/, int multiplier);

    MODULE_API int serialReadUntil(int64_t handle, void* buffer, int bufferSize, int timeout, int multiplier, void* untilChar);

    MODULE_API int serialWrite(int64_t handle, const void* buffer, int bufferSize, int timeout, int multiplier);

    // Enumerate ports; callback gets simple COM name first (e.g. "COM3"),
    // followed by the full device path and further meta-data.
    MODULE_API int serialGetPortsInfo(void (*function)(const char* port,
                                                       const char* path,
                                                       const char* manufacturer,
                                                       const char* serialNumber,
                                                       const char* pnpId,
                                                       const char* locationId,
                                                       const char* productId,
                                                       const char* vendorId));

    MODULE_API void serialClearBufferIn(int64_t handle);
    MODULE_API void serialClearBufferOut(int64_t handle);
    MODULE_API void serialAbortRead(int64_t handle);
    MODULE_API void serialAbortWrite(int64_t handle);

    // Optional callback hooks (can be nullptr)
    extern void (*on_read_callback)(int bytes);
    extern void (*on_write_callback)(int bytes);
    extern void (*on_error_callback)(int errorCode, const char* message);

    MODULE_API void serialOnRead(void (*func)(int bytes));
    MODULE_API void serialOnWrite(void (*func)(int bytes));
    MODULE_API void serialOnError(void (*func)(int code, const char* message));

    MODULE_API int serialReadLine(int64_t handle, void* buffer, int bufferSize, int timeout /*ms*/);

    MODULE_API int serialWriteLine(int64_t handle, const void* buffer, int bufferSize, int timeout /*ms*/);

    MODULE_API int serialReadUntilSequence(int64_t handle, void* buffer, int bufferSize, int timeout /*ms*/, void* sequence);

    MODULE_API int serialReadFrame(int64_t handle, void* buffer, int bufferSize, int timeout /*ms*/, char startByte, char endByte);

    // Byte statistics
    MODULE_API int64_t serialOutBytesTotal(int64_t handle);
    MODULE_API int64_t serialInBytesTotal(int64_t handle);

    // Drain pending TX bytes (wait until sent)
    MODULE_API int serialDrain(int64_t handle);

    // Bytes currently queued in the driver buffers
    MODULE_API int serialInBytesWaiting(int64_t handle);
    MODULE_API int serialOutBytesWaiting(int64_t handle);

#ifdef __cplusplus
}
#endif
