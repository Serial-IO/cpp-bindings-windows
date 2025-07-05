#include "serial.h"
#include "status_codes.h"

// clang-format off
#include <windows.h>
// clang-format on

#include <cstring>
#include <guiddef.h>
#include <setupapi.h> // requires windows.h but does not include it
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "setupapi.lib")

static void invokeErrorLocal(int code, const char* message)
{
    if (error_callback != nullptr)
    {
        error_callback(code, message);
    }
}

// Extract VID, PID, Serial from a HardwareID or device path string
static void parseVidPidSerial(const std::string& src, std::string& vid, std::string& pid, std::string& serial)
{
    auto pos_vid = src.find("VID_");
    if (pos_vid != std::string::npos && pos_vid + 8 <= src.size())
    {
        vid = src.substr(pos_vid + 4, 4);
    }

    auto pos_pid = src.find("PID_");
    if (pos_pid != std::string::npos && pos_pid + 8 <= src.size())
    {
        pid = src.substr(pos_pid + 4, 4);
    }

    // Serial number is heuristic: substring between second and third '#'
    size_t first_hash = src.find('#');
    size_t second_hash = src.find('#', first_hash + 1);
    size_t third_hash = src.find('#', second_hash + 1);
    if (second_hash != std::string::npos && third_hash != std::string::npos && third_hash > second_hash)
    {
        serial = src.substr(second_hash + 1, third_hash - second_hash - 1);
    }
}

extern "C" int serialGetPortsInfo(void (*function)(const char* port,
                                                   const char* path,
                                                   const char* manufacturer,
                                                   const char* serialNumber,
                                                   const char* pnpId,
                                                   const char* locationId,
                                                   const char* productId,
                                                   const char* vendorId))
{
    if (function == nullptr)
    {
        invokeErrorLocal(std::to_underlying(StatusCodes::BUFFER_ERROR), "serialGetPortsInfo: function pointer is null");
        return 0;
    }

    const GUID guid_devinterface_comport = {0x86E0D1E0, 0x8089, 0x11D0, {0x9C, 0xE4, 0x08, 0x00, 0x3E, 0x30, 0x1F, 0x73}};

    HDEVINFO h_dev_info = SetupDiGetClassDevs(&guid_devinterface_comport, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (h_dev_info == INVALID_HANDLE_VALUE)
    {
        invokeErrorLocal(std::to_underlying(StatusCodes::NOT_FOUND_ERROR), "serialGetPortsInfo: SetupDiGetClassDevs failed");
        return 0;
    }

    int count = 0;
    for (DWORD index = 0;; ++index)
    {
        SP_DEVICE_INTERFACE_DATA iface_data{sizeof(iface_data)};
        if (SetupDiEnumDeviceInterfaces(h_dev_info, nullptr, &guid_devinterface_comport, index, &iface_data) == 0)
        {
            if (GetLastError() == ERROR_NO_MORE_ITEMS)
            {
                break;
            }
            continue;
        }

        // Get interface detail (device path)
        DWORD req_size = 0;
        SetupDiGetDeviceInterfaceDetail(h_dev_info, &iface_data, nullptr, 0, &req_size, nullptr);
        std::vector<char> buf(req_size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        SP_DEVINFO_DATA dev_info{sizeof(dev_info)};
        if (SetupDiGetDeviceInterfaceDetailA(h_dev_info, &iface_data, detail, req_size, nullptr, &dev_info) == 0)
        {
            continue;
        }

        std::string device_path = detail->DevicePath;

        // Friendly name (contains "(COMx)")
        CHAR friendly[256] = "";
        SetupDiGetDeviceRegistryPropertyA(
            h_dev_info, &dev_info, SPDRP_FRIENDLYNAME, nullptr, reinterpret_cast<BYTE*>(friendly), sizeof(friendly), nullptr);

        std::string com_name;
        const char* paren = std::strchr(friendly, '(');
        if (paren != nullptr)
        {
            const char* end_paren = std::strchr(paren, ')');
            if (end_paren != nullptr && end_paren > paren + 1)
            {
                com_name.assign(paren, static_cast<size_t>(end_paren - paren - 1));
            }
        }

        // Manufacturer
        CHAR mfg[256] = "";
        SetupDiGetDeviceRegistryPropertyA(h_dev_info, &dev_info, SPDRP_MFG, nullptr, reinterpret_cast<BYTE*>(mfg), sizeof(mfg), nullptr);

        // Location information
        CHAR loc[256] = "";
        SetupDiGetDeviceRegistryPropertyA(
            h_dev_info, &dev_info, SPDRP_LOCATION_INFORMATION, nullptr, reinterpret_cast<BYTE*>(loc), sizeof(loc), nullptr);

        // Hardware ID (multi-sz) → first string
        CHAR hwid[256] = "";
        SetupDiGetDeviceRegistryPropertyA(
            h_dev_info, &dev_info, SPDRP_HARDWAREID, nullptr, reinterpret_cast<BYTE*>(hwid), sizeof(hwid), nullptr);

        std::string vid;
        std::string pid;
        std::string serial;
        parseVidPidSerial(hwid, vid, pid, serial);
        if (serial.empty())
        {
            parseVidPidSerial(device_path, vid, pid, serial);
        }

        function(com_name.c_str(),
                 device_path.c_str(),
                 (*mfg != 0) ? mfg : "",
                 serial.c_str(),
                 (hwid[0] != 0) ? hwid : "",
                 (loc[0] != 0) ? loc : "",
                 pid.c_str(),
                 vid.c_str());
        ++count;
    }

    SetupDiDestroyDeviceInfoList(h_dev_info);
    return count;
}
