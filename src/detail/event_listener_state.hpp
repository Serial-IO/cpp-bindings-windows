#pragma once

#include <cpp_core/strong_types.hpp>

#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <utility>

namespace cpp_bindings_windows::detail
{
// Only the dispatcher accesses the port map. Native notifications copy their
// interface paths into the queue and never invoke application code.
class EventListenerState
{
  public:
    using PortMap = std::map<std::wstring, std::string>;

    void enqueue(cpp_core::PortEvent event, std::wstring path)
    {
        std::lock_guard lock(mutex_);
        if (!stopped_)
        {
            pending_.push_back({event, std::move(path)});
            wakeup_.notify_one();
        }
    }

    void reportQueueFailure()
    {
        std::lock_guard lock(mutex_);
        queue_failed_ = true;
        wakeup_.notify_one();
    }

    void activate()
    {
        std::lock_guard lock(mutex_);
        active_ = true;
        wakeup_.notify_one();
    }

    void stop()
    {
        std::lock_guard lock(mutex_);
        stopped_ = true;
        wakeup_.notify_one();
    }

    template <typename ResolvePort, typename Callback, typename ReportError>
    void run(PortMap ports, ResolvePort resolve_port, Callback callback, ReportError report_error)
    {
        for (;;)
        {
            std::unique_lock lock(mutex_);
            wakeup_.wait(lock, [this] { return stopped_ || (active_ && (queue_failed_ || !pending_.empty())); });
            if (stopped_)
            {
                return;
            }
            if (queue_failed_)
            {
                queue_failed_ = false;
                lock.unlock();
                report_error("Could not queue a serial device notification");
                continue;
            }
            auto notification = std::move(pending_.front());
            pending_.pop_front();
            lock.unlock();

            try
            {
                const auto found = ports.find(notification.path);
                if (notification.event == cpp_core::PortEvent::kAttached)
                {
                    // Registration precedes the initial enumeration, so an arrival
                    // can be present in both the initial map and the queue.
                    if (found == ports.end())
                    {
                        std::string port = resolve_port(notification.path);
                        ports.emplace(notification.path, port);
                        callback(notification.event, port.c_str());
                    }
                }
                else if (found != ports.end())
                {
                    // The device may already be gone; use its remembered COM name.
                    auto port = std::move(found->second);
                    ports.erase(found);
                    callback(notification.event, port.c_str());
                }
            }
            catch (const std::exception &error)
            {
                report_error(error.what());
            }
        }
    }

  private:
    struct Notification
    {
        cpp_core::PortEvent event;
        std::wstring path;
    };

    std::mutex mutex_;
    std::condition_variable wakeup_;
    std::deque<Notification> pending_;
    bool active_ = false;
    bool stopped_ = false;
    bool queue_failed_ = false;
};
} // namespace cpp_bindings_windows::detail
