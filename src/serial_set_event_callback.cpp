#include <cpp_core/interface/serial_set_event_callback.h>

#include "detail/fail_win32.hpp"
#include "detail/win32_error_to_string.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace
{
std::mutex g_monitor_mutex;
std::mutex g_wait_mutex;
std::condition_variable_any g_wakeup;
std::jthread g_monitor_thread;

auto enumerateComPorts() -> std::optional<std::set<std::string>>
{
    std::vector<char> buffer(65536);
    const DWORD length = QueryDosDeviceA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0)
    {
        return std::nullopt;
    }

    std::set<std::string> ports;
    const char *current = buffer.data();
    while (*current != '\0')
    {
        std::string name(current);
        if (name.size() >= 4 && (name.starts_with("COM") || name.starts_with("com")))
        {
            ports.insert(std::move(name));
        }
        current += std::char_traits<char>::length(current) + 1;
    }
    return ports;
}

auto monitorLoop(std::stop_token stop_token, std::set<std::string> previous,
                 void (*callback)(cpp_core::PortEvent event, const char *port), ErrorCallbackT error_callback) -> void
{
    std::unique_lock wait_lock(g_wait_mutex);
    while (!stop_token.stop_requested())
    {
        (void)g_wakeup.wait_for(wait_lock, stop_token, std::chrono::milliseconds(500), [] { return false; });
        if (stop_token.stop_requested())
        {
            break;
        }

        wait_lock.unlock();
        auto current = enumerateComPorts();
        if (!current)
        {
            cpp_core::invokeError(error_callback,
                                  static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Monitor::kMonitorError),
                                  cpp_bindings_windows::detail::win32ErrorToString(GetLastError()));
            wait_lock.lock();
            continue;
        }

        for (const auto &port : *current)
        {
            if (!stop_token.stop_requested() && !previous.contains(port))
            {
                callback(cpp_core::PortEvent::kAttached, port.c_str());
            }
        }
        for (const auto &port : previous)
        {
            if (!stop_token.stop_requested() && !current->contains(port))
            {
                callback(cpp_core::PortEvent::kDetached, port.c_str());
            }
        }
        previous = std::move(*current);
        wait_lock.lock();
    }
}
} // namespace

extern "C"
{

    MODULE_API auto serialSetEventCallback(void (*callback_function)(cpp_core::PortEvent event, const char *port),
                                           ErrorCallbackT error_callback) -> int
    {
        const auto callback = cpp_bindings_windows::detail::effectiveErrorCallback(error_callback);
        std::jthread replacement;
        if (callback_function != nullptr)
        {
            auto initial_ports = enumerateComPorts();
            if (!initial_ports)
            {
                return cpp_bindings_windows::detail::failWin32<int>(
                    callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Monitor::kMonitorError));
            }
            try
            {
                replacement = std::jthread(monitorLoop, std::move(*initial_ports), callback_function, callback);
            }
            catch (const std::system_error &error)
            {
                return cpp_core::failMsg<int>(
                    callback, static_cast<cpp_core::StatusCodeValue>(cpp_core::StatusCode::Monitor::kMonitorError),
                    error.what());
            }
        }

        std::jthread previous;
        {
            std::lock_guard lock(g_monitor_mutex);
            previous = std::move(g_monitor_thread);
            g_monitor_thread = std::move(replacement);
            previous.request_stop();
            g_wakeup.notify_all();
        }
        // Join after releasing the mutex, so a callback may clear or replace itself.
        if (previous.joinable() && previous.get_id() == std::this_thread::get_id())
        {
            previous.detach();
        }
        return static_cast<int>(cpp_core::StatusCode::kSuccess);
    }

} // extern "C"
