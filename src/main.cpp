#include <windows.h>
#include <string>
#include <stdexcept>
#include <vector>

class SerialPort {
private:
    HANDLE hSerial;
    OVERLAPPED readOverlap;
    OVERLAPPED writeOverlap;

public:
    SerialPort() : hSerial(INVALID_HANDLE_VALUE) {
        ZeroMemory(&readOverlap, sizeof(OVERLAPPED));
        ZeroMemory(&writeOverlap, sizeof(OVERLAPPED));
        readOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        writeOverlap.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    }

    ~SerialPort() {
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
        }
        if (readOverlap.hEvent) CloseHandle(readOverlap.hEvent);
        if (writeOverlap.hEvent) CloseHandle(writeOverlap.hEvent);
    }

    bool open(const char* portName, DWORD baudRate) {
        std::string port = "\\\\.\\";
        port += portName;
        
        hSerial = CreateFileA(port.c_str(),
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            OPEN_EXISTING,
                            FILE_FLAG_OVERLAPPED,
                            NULL);

        if (hSerial == INVALID_HANDLE_VALUE) {
            return false;
        }

        DCB dcbSerialParams = { 0 };
        dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
        
        if (!GetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        dcbSerialParams.BaudRate = baudRate;
        dcbSerialParams.ByteSize = 8;
        dcbSerialParams.StopBits = ONESTOPBIT;
        dcbSerialParams.Parity = NOPARITY;

        if (!SetCommState(hSerial, &dcbSerialParams)) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        COMMTIMEOUTS timeouts = { 0 };
        timeouts.ReadIntervalTimeout = MAXDWORD;
        timeouts.ReadTotalTimeoutMultiplier = 0;
        timeouts.ReadTotalTimeoutConstant = 0;
        timeouts.WriteTotalTimeoutMultiplier = 0;
        timeouts.WriteTotalTimeoutConstant = 0;

        if (!SetCommTimeouts(hSerial, &timeouts)) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return false;
        }

        return true;
    }

    bool close() {
        if (hSerial != INVALID_HANDLE_VALUE) {
            CloseHandle(hSerial);
            hSerial = INVALID_HANDLE_VALUE;
            return true;
        }
        return false;
    }

    int readAsync(BYTE* buffer, DWORD bufferSize) {
        if (hSerial == INVALID_HANDLE_VALUE) {
            return 0;
        }

        DWORD bytesRead = 0;
        ResetEvent(readOverlap.hEvent);
        
        if (!ReadFile(hSerial, buffer, bufferSize, &bytesRead, &readOverlap)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(readOverlap.hEvent, INFINITE) == WAIT_OBJECT_0) {
                    GetOverlappedResult(hSerial, &readOverlap, &bytesRead, FALSE);
                }
            }
        }
        return bytesRead;
    }

    int writeAsync(const BYTE* buffer, DWORD bufferSize) {
        if (hSerial == INVALID_HANDLE_VALUE) {
            return 0;
        }

        DWORD bytesWritten = 0;
        ResetEvent(writeOverlap.hEvent);
        
        if (!WriteFile(hSerial, buffer, bufferSize, &bytesWritten, &writeOverlap)) {
            if (GetLastError() == ERROR_IO_PENDING) {
                if (WaitForSingleObject(writeOverlap.hEvent, INFINITE) == WAIT_OBJECT_0) {
                    GetOverlappedResult(hSerial, &writeOverlap, &bytesWritten, FALSE);
                }
            }
        }
        return bytesWritten;
    }
};

static SerialPort* port = nullptr;

extern "C" {
    __declspec(dllexport) int OpenSerialPort(const char* portName, DWORD baudRate) {
        try {
            if (port != nullptr) {
                delete port;
            }
            port = new SerialPort();
            return port->open(portName, baudRate) ? 1 : 0;
        }
        catch (...) {
            return 0;
        }
    }

    __declspec(dllexport) int CloseSerialPort() {
        try {
            if (port != nullptr) {
                bool result = port->close();
                delete port;
                port = nullptr;
                return result ? 1 : 0;
            }
            return 0;
        }
        catch (...) {
            return 0;
        }
    }

    __declspec(dllexport) int ReadSerialPort(BYTE* buffer, DWORD bufferSize) {
        try {
            if (port == nullptr) {
                return 0;
            }
            return port->readAsync(buffer, bufferSize);
        }
        catch (...) {
            return 0;
        }
    }

    __declspec(dllexport) int WriteSerialPort(const BYTE* buffer, DWORD bufferSize) {
        try {
            if (port == nullptr) {
                return 0;
            }
            return port->writeAsync(buffer, bufferSize);
        }
        catch (...) {
            return 0;
        }
    }
}
