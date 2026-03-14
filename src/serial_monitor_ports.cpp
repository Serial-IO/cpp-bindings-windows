#include <cpp_core/interface/serial_monitor_ports.h>
#include <cpp_core/status_codes.h>

#include "detail/win32_helpers.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace
{

std::mutex g_mutex;
std::thread g_thread;
HANDLE g_stop_event = nullptr;
std::atomic<bool> g_running{false};

auto enumerateComPorts() -> std::set<std::string>
{
    std::set<std::string> ports;
    std::vector<char> buffer(65536);
    const DWORD len = QueryDosDeviceA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len == 0)
    {
        return ports;
    }

    const char *ptr = buffer.data();
    while (*ptr != '\0')
    {
        std::string name(ptr);
        if (name.rfind("COM", 0) == 0 && name.size() >= 4)
        {
            ports.insert(name);
        }
        ptr += name.size() + 1;
    }
    return ports;
}

void monitorLoop(void (*callback)(int event, const char *port))
{
    std::set<std::string> previous = enumerateComPorts();

    while (g_running.load(std::memory_order_relaxed))
    {
        const DWORD wait = WaitForSingleObject(g_stop_event, 500);
        if (wait == WAIT_OBJECT_0)
        {
            break;
        }

        std::set<std::string> current = enumerateComPorts();

        for (const auto &p : current)
        {
            if (previous.find(p) == previous.end())
            {
                callback(1, p.c_str());
            }
        }

        for (const auto &p : previous)
        {
            if (current.find(p) == current.end())
            {
                callback(0, p.c_str());
            }
        }

        previous = std::move(current);
    }
}

void stopMonitor()
{
    if (!g_running.load(std::memory_order_relaxed))
    {
        return;
    }

    g_running.store(false, std::memory_order_relaxed);

    if (g_stop_event != nullptr)
    {
        SetEvent(g_stop_event);
    }

    if (g_thread.joinable())
    {
        g_thread.join();
    }

    if (g_stop_event != nullptr)
    {
        CloseHandle(g_stop_event);
        g_stop_event = nullptr;
    }
}

} // namespace

extern "C"
{

    MODULE_API auto serialMonitorPorts(void (*callback_fn)(int event, const char *port),
                                       ErrorCallbackT error_callback) -> int
    {
        std::lock_guard lock(g_mutex);

        stopMonitor();

        if (callback_fn == nullptr)
        {
            return 0;
        }

        g_stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (g_stop_event == nullptr)
        {
            return cpp_bindings_windows::detail::failWin32<int>(error_callback, cpp_core::StatusCodes::kMonitorError);
        }

        g_running.store(true, std::memory_order_relaxed);
        g_thread = std::thread(monitorLoop, callback_fn);

        return 0;
    }

} // extern "C"
