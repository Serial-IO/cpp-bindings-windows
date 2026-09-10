#include <cpp_core/error_handling.hpp>
#include <cpp_core/interface/serial_set_event_callback.h>
#include <cpp_core/scope_guard.hpp>

#include "detail/effective_error_callback.hpp"
#include "detail/event_listener_state.hpp"
#include "detail/wide_to_utf8.hpp"

#include <cfgmgr32.h>
#include <ntddser.h>
#include <setupapi.h>

#include <algorithm>
#include <cwchar>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <thread>
#include <vector>

namespace
{
using cpp_bindings_windows::detail::EventListenerState;
using EventCallback = void (*)(cpp_core::PortEvent, const char *);
constexpr auto kListenerError = static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Monitor::kMonitorError);

void checkConfigResult(CONFIGRET result)
{
    if (result != CR_SUCCESS)
    {
        throw std::system_error(static_cast<int>(CM_MapCrToWin32Err(result, ERROR_GEN_FAILURE)),
                                std::system_category());
    }
}

void throwLastError()
{
    throw std::system_error(static_cast<int>(GetLastError()), std::system_category());
}

auto interfaceKey(std::wstring path) -> std::wstring
{
    // Device interface paths are case insensitive. Enumeration and notifications
    // need to use the same map key even when their spelling differs.
    if (!path.empty())
    {
        CharUpperBuffW(path.data(), static_cast<DWORD>(path.size()));
    }
    return path;
}

auto portName(const std::wstring &path) -> std::string
{
    const auto devices = SetupDiCreateDeviceInfoList(nullptr, nullptr);
    if (devices == INVALID_HANDLE_VALUE)
    {
        throwLastError();
    }
    const auto cleanup = cpp_core::defer([&] { SetupDiDestroyDeviceInfoList(devices); });
    SP_DEVICE_INTERFACE_DATA device_interface{};
    device_interface.cbSize = sizeof(device_interface);
    if (!SetupDiOpenDeviceInterfaceW(devices, path.c_str(), 0, &device_interface))
    {
        throwLastError();
    }
    SP_DEVINFO_DATA device{};
    device.cbSize = sizeof(device);
    // This size query also populates the device information; no path buffer is needed.
    if (!SetupDiGetDeviceInterfaceDetailW(devices, &device_interface, nullptr, 0, nullptr, &device) &&
        GetLastError() != ERROR_INSUFFICIENT_BUFFER)
    {
        throwLastError();
    }
    const HKEY key = SetupDiOpenDevRegKey(devices, &device, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_QUERY_VALUE);
    if (key == INVALID_HANDLE_VALUE)
    {
        throwLastError();
    }
    const auto close_key = cpp_core::defer([&] { RegCloseKey(key); });
    DWORD bytes = 0;
    auto result = RegGetValueW(key, nullptr, L"PortName", RRF_RT_REG_SZ, nullptr, nullptr, &bytes);
    if (result != ERROR_SUCCESS)
    {
        throw std::system_error(result, std::system_category());
    }
    std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');
    result = RegGetValueW(key, nullptr, L"PortName", RRF_RT_REG_SZ, nullptr, buffer.data(), &bytes);
    if (result != ERROR_SUCCESS)
    {
        throw std::system_error(result, std::system_category());
    }
    const auto port = cpp_bindings_windows::detail::wideToUtf8(buffer.data());
    if (port.empty())
    {
        throw std::runtime_error("Serial device has no COM port name");
    }
    return port;
}

auto existingPorts() -> EventListenerState::PortMap
{
    std::vector<wchar_t> interfaces;
    CONFIGRET result;
    do
    {
        ULONG length = 0;
        checkConfigResult(CM_Get_Device_Interface_List_SizeW(&length, const_cast<GUID *>(&GUID_DEVINTERFACE_COMPORT),
                                                             nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT));
        interfaces.assign(std::max<ULONG>(length, 1), L'\0');
        result =
            CM_Get_Device_Interface_ListW(const_cast<GUID *>(&GUID_DEVINTERFACE_COMPORT), nullptr, interfaces.data(),
                                          static_cast<ULONG>(interfaces.size()), CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    } while (result == CR_BUFFER_SMALL);
    checkConfigResult(result);

    EventListenerState::PortMap ports;
    for (const wchar_t *path = interfaces.data(); *path != L'\0'; path += std::wcslen(path) + 1)
    {
        try
        {
            ports.emplace(interfaceKey(path), portName(path));
        }
        catch (const std::system_error &error)
        {
            // An interface can disappear while the initial snapshot is being built.
            // Its queued notification will be handled by the dispatcher.
            const auto code = static_cast<DWORD>(error.code().value());
            if (code != ERROR_NO_SUCH_DEVICE_INTERFACE && code != ERROR_NO_SUCH_DEVINST &&
                code != ERROR_DEV_NOT_EXIST && code != ERROR_FILE_NOT_FOUND && code != ERROR_PATH_NOT_FOUND &&
                code != ERROR_KEY_DELETED)
            {
                throw;
            }
        }
    }
    return ports;
}

DWORD CALLBACK deviceNotification(HCMNOTIFICATION, void *context, CM_NOTIFY_ACTION action, CM_NOTIFY_EVENT_DATA *data,
                                  DWORD)
{
    if (data->FilterType != CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE ||
        (action != CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL && action != CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL))
    {
        return ERROR_SUCCESS;
    }
    auto &state = *static_cast<EventListenerState *>(context);
    try
    {
        state.enqueue(action == CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL ? cpp_core::PortEvent::kAttached
                                                                        : cpp_core::PortEvent::kDetached,
                      interfaceKey(data->u.DeviceInterface.SymbolicLink));
    }
    catch (...)
    {
        // No C++ exceptions or application callbacks may escape into the PnP callback.
        state.reportQueueFailure();
        return ERROR_NOT_ENOUGH_MEMORY;
    }
    return ERROR_SUCCESS;
}

struct EventListener
{
    std::shared_ptr<EventListenerState> state = std::make_shared<EventListenerState>();
    HCMNOTIFICATION notification = nullptr;
    std::thread thread;

    ~EventListener()
    {
        state->stop();
        if (notification != nullptr)
        {
            // Native callbacks only enqueue, so unregistering cannot wait on user code.
            CM_Unregister_Notification(notification);
        }
        if (thread.joinable())
        {
            if (thread.get_id() == std::this_thread::get_id())
            {
                // The dispatcher retains the state until this user callback returns.
                thread.detach();
            }
            else
            {
                thread.join();
            }
        }
    }
};

std::mutex g_event_listener_mutex;
std::unique_ptr<EventListener> g_event_listener;
} // namespace

MODULE_API auto serialSetEventCallback(EventCallback callback_function, ErrorCallbackT error_callback) -> int
{
    const auto error_handler = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
    std::unique_ptr<EventListener> replacement;
    try
    {
        if (callback_function != nullptr)
        {
            replacement = std::make_unique<EventListener>();
            CM_NOTIFY_FILTER filter{};
            filter.cbSize = sizeof(filter);
            filter.FilterType = CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
            filter.u.DeviceInterface.ClassGuid = GUID_DEVINTERFACE_COMPORT;
            checkConfigResult(CM_Register_Notification(&filter, replacement->state.get(), deviceNotification,
                                                       &replacement->notification));
            // Register first to avoid losing changes during enumeration. Dispatch starts
            // only once this listener is installed as the active registration.
            auto ports = existingPorts();
            replacement->thread = std::thread(
                [state = replacement->state, ports = std::move(ports), callback_function, error_handler]() mutable {
                    state->run(std::move(ports), portName, callback_function, [error_handler](const char *message) {
                        cpp_core::invokeError(error_handler, kListenerError, message);
                    });
                });
        }
    }
    catch (const std::exception &error)
    {
        replacement.reset();
        return cpp_core::failMsg<int>(error_handler, kListenerError, error.what());
    }

    std::unique_ptr<EventListener> previous;
    {
        std::lock_guard lock(g_event_listener_mutex);
        previous = std::move(g_event_listener);
        if (previous)
        {
            previous->state->stop();
        }
        g_event_listener = std::move(replacement);
        if (g_event_listener)
        {
            g_event_listener->state->activate();
        }
    }
    // Destruction unregisters and joins outside the lock: callbacks may replace or clear themselves.
    previous.reset();
    return static_cast<int>(cpp_core::StatusCode::kSuccess);
}
