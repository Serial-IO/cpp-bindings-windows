#include <cpp_core/interface/serial_list_ports.h>
#include <cpp_core/scope_guard.hpp>

#include "detail/handle_state.hpp"
#include "detail/win32_helpers.hpp"

#include <devguid.h>
#include <setupapi.h>

#include <algorithm>
#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace
{
struct PortInfo
{
    std::string port;
    std::string path;
    std::string manufacturer;
    std::string serial_number;
    std::string pnp_id;
    std::string location_id;
    std::string product_id;
    std::string vendor_id;
};

auto registryString(HKEY key, const wchar_t *value_name) -> std::optional<std::wstring>
{
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || size < sizeof(wchar_t))
    {
        return std::nullopt;
    }

    std::vector<wchar_t> buffer(size / sizeof(wchar_t));
    if (RegQueryValueExW(key, value_name, nullptr, &type, reinterpret_cast<BYTE *>(buffer.data()), &size) !=
        ERROR_SUCCESS)
    {
        return std::nullopt;
    }
    return std::wstring(buffer.data());
}

auto portName(HDEVINFO device_info_set, SP_DEVINFO_DATA *device_info) -> std::optional<std::wstring>
{
    HKEY key = SetupDiOpenDevRegKey(device_info_set, device_info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
    if (key == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }
    const auto value = registryString(key, L"PortName");
    RegCloseKey(key);
    return value;
}

auto deviceProperty(HDEVINFO device_info_set, SP_DEVINFO_DATA *device_info, DWORD property)
    -> std::optional<std::wstring>
{
    DWORD type = 0;
    DWORD size = 0;
    (void)SetupDiGetDeviceRegistryPropertyW(device_info_set, device_info, property, &type, nullptr, 0, &size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size < sizeof(wchar_t))
    {
        return std::nullopt;
    }

    std::vector<BYTE> buffer(size);
    if (SetupDiGetDeviceRegistryPropertyW(device_info_set, device_info, property, &type, buffer.data(), size,
                                          nullptr) == 0)
    {
        return std::nullopt;
    }
    return std::wstring(reinterpret_cast<const wchar_t *>(buffer.data()));
}

auto instanceId(HDEVINFO device_info_set, SP_DEVINFO_DATA *device_info) -> std::optional<std::wstring>
{
    DWORD required = 0;
    (void)SetupDiGetDeviceInstanceIdW(device_info_set, device_info, nullptr, 0, &required);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || required == 0)
    {
        return std::nullopt;
    }

    std::vector<wchar_t> buffer(required);
    if (SetupDiGetDeviceInstanceIdW(device_info_set, device_info, buffer.data(), required, nullptr) == 0)
    {
        return std::nullopt;
    }
    return std::wstring(buffer.data());
}

auto asciiUpper(std::string value) -> std::string
{
    std::ranges::transform(value, value.begin(),
                           [](unsigned char character) { return static_cast<char>(std::toupper(character)); });
    return value;
}

auto hardwareId(std::string_view pnp_id, std::string_view prefix) -> std::string
{
    const std::string upper = asciiUpper(std::string(pnp_id));
    const auto position = upper.find(prefix);
    if (position == std::string::npos || position + prefix.size() + 4 > upper.size())
    {
        return {};
    }
    return upper.substr(position + prefix.size(), 4);
}

auto serialNumber(std::string_view pnp_id) -> std::string
{
    const auto separator = pnp_id.rfind('\\');
    if (separator == std::string_view::npos || separator + 1 >= pnp_id.size())
    {
        return {};
    }

    std::string candidate(pnp_id.substr(separator + 1));
    return candidate.find('&') == std::string::npos ? candidate : std::string{};
}

auto optionalCString(const std::string &value) -> const char *
{
    return value.empty() ? nullptr : value.c_str();
}
} // namespace

extern "C"
{

    MODULE_API auto serialListPorts(void (*callback_fn)(const char *port, const char *path, const char *manufacturer,
                                                        const char *serial_number, const char *pnp_id,
                                                        const char *location_id, const char *product_id,
                                                        const char *vendor_id),
                                    ErrorCallbackT error_callback) -> int
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        if (callback_fn == nullptr)
        {
            return cpp_core::failMsg<int>(
                callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Io::kBufferError),
                "Port callback must not be null");
        }

        const HDEVINFO device_info_set = SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS, nullptr, nullptr, DIGCF_PRESENT);
        if (device_info_set == INVALID_HANDLE_VALUE)
        {
            return cpp_bindings_windows::detail::failWin32<int>(
                callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Monitor::kMonitorError));
        }
        const auto cleanup = cpp_core::defer([&] { SetupDiDestroyDeviceInfoList(device_info_set); });

        std::vector<PortInfo> ports;
        for (DWORD index = 0;; ++index)
        {
            SP_DEVINFO_DATA device_info = {};
            device_info.cbSize = sizeof(device_info);
            if (SetupDiEnumDeviceInfo(device_info_set, index, &device_info) == 0)
            {
                if (GetLastError() == ERROR_NO_MORE_ITEMS)
                {
                    break;
                }
                return cpp_bindings_windows::detail::failWin32<int>(
                    callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Monitor::kMonitorError));
            }

            const auto port_name = portName(device_info_set, &device_info);
            if (!port_name || port_name->size() < 4 ||
                (!port_name->starts_with(L"COM") && !port_name->starts_with(L"com")))
            {
                continue;
            }

            PortInfo info;
            info.port = cpp_bindings_windows::detail::wideToUtf8(*port_name);
            info.path = "\\\\.\\" + info.port;
            if (const auto value = deviceProperty(device_info_set, &device_info, SPDRP_MFG))
            {
                info.manufacturer = cpp_bindings_windows::detail::wideToUtf8(*value);
            }
            if (const auto value = deviceProperty(device_info_set, &device_info, SPDRP_LOCATION_INFORMATION))
            {
                info.location_id = cpp_bindings_windows::detail::wideToUtf8(*value);
            }
            if (const auto value = instanceId(device_info_set, &device_info))
            {
                info.pnp_id = cpp_bindings_windows::detail::wideToUtf8(*value);
                info.serial_number = serialNumber(info.pnp_id);
                info.vendor_id = hardwareId(info.pnp_id, "VID_");
                info.product_id = hardwareId(info.pnp_id, "PID_");
            }
            ports.push_back(std::move(info));
        }

        std::ranges::sort(ports, {}, &PortInfo::port);
        for (const auto &info : ports)
        {
            callback_fn(optionalCString(info.port), optionalCString(info.path), optionalCString(info.manufacturer),
                        optionalCString(info.serial_number), optionalCString(info.pnp_id),
                        optionalCString(info.location_id), optionalCString(info.product_id),
                        optionalCString(info.vendor_id));
        }
        return static_cast<int>(ports.size());
    }

} // extern "C"
