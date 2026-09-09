#include <cpp_core/interface/serial_set_event_callback.h>

#include "detail/event_listener_state.hpp"

#include <chrono>
#include <cstddef>
#include <future>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using cpp_bindings_windows::detail::EventListenerState;
using cpp_core::PortEvent;
using Event = std::pair<PortEvent, std::string>;

class DispatcherThread
{
  public:
    template <typename Function>
    DispatcherThread(EventListenerState &state, Function function) : state_(state), thread_(std::move(function))
    {
    }

    ~DispatcherThread()
    {
        state_.stop();
        thread_.join();
    }

  private:
    EventListenerState &state_;
    std::thread thread_;
};
} // namespace

TEST(SerialEventListenerTest, QueuesRapidAttachAndDetachAndRemembersRemovedPortName)
{
    EventListenerState state;
    state.enqueue(PortEvent::kAttached, L"device-a");
    state.enqueue(PortEvent::kDetached, L"device-a");
    state.enqueue(PortEvent::kAttached, L"device-a");
    state.enqueue(PortEvent::kDetached, L"device-a");
    state.activate();

    int resolutions = 0;
    std::vector<Event> events;
    state.run(
        {},
        [&](const std::wstring &) {
            ++resolutions;
            return resolutions == 1 ? "COM3" : "COM7";
        },
        [&](PortEvent event, const char *port) {
            events.emplace_back(event, port);
            if (events.size() == 4)
            {
                state.stop();
            }
        },
        [](const char *message) { FAIL() << message; });

    EXPECT_EQ(resolutions, 2);
    EXPECT_EQ(events, (std::vector<Event>{{PortEvent::kAttached, "COM3"},
                                          {PortEvent::kDetached, "COM3"},
                                          {PortEvent::kAttached, "COM7"},
                                          {PortEvent::kDetached, "COM7"}}));
}

TEST(SerialEventListenerTest, InitialSnapshotDeduplicatesArrivalsAndUnknownRemovals)
{
    EventListenerState state;
    state.enqueue(PortEvent::kAttached, L"existing");
    state.enqueue(PortEvent::kAttached, L"existing");
    state.enqueue(PortEvent::kDetached, L"unknown");
    state.enqueue(PortEvent::kDetached, L"existing");
    state.activate();

    std::vector<Event> events;
    state.run(
        {{L"existing", "COM4"}},
        [](const std::wstring &) -> std::string {
            ADD_FAILURE() << "Existing and removed ports must use the snapshot";
            return {};
        },
        [&](PortEvent event, const char *port) {
            events.emplace_back(event, port);
            state.stop();
        },
        [](const char *message) { FAIL() << message; });

    EXPECT_EQ(events, (std::vector<Event>{{PortEvent::kDetached, "COM4"}}));
}

TEST(SerialEventListenerTest, CallbackCanStopDispatcherAndDiscardQueuedEvents)
{
    EventListenerState state;
    state.enqueue(PortEvent::kAttached, L"first");
    state.enqueue(PortEvent::kAttached, L"second");
    state.activate();

    int callbacks = 0;
    state.run(
        {}, [](const std::wstring &) { return "COM3"; },
        [&](PortEvent, const char *) {
            ++callbacks;
            state.stop();
            state.enqueue(PortEvent::kAttached, L"after-stop");
        },
        [](const char *message) { FAIL() << message; });
    EXPECT_EQ(callbacks, 1);
}

TEST(SerialEventListenerTest, DoesNotDispatchUntilRegistrationIsActivated)
{
    EventListenerState state;
    state.enqueue(PortEvent::kAttached, L"device");
    std::promise<void> delivered;
    auto completion = delivered.get_future();
    DispatcherThread dispatcher(state, [&] {
        state.run(
            {}, [](const std::wstring &) { return "COM3"; },
            [&](PortEvent, const char *) {
                state.stop();
                delivered.set_value();
            },
            [](const char *message) { FAIL() << message; });
    });
    EXPECT_EQ(completion.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    state.activate();
    EXPECT_EQ(completion.wait_for(std::chrono::seconds(2)), std::future_status::ready);
}

TEST(SerialEventListenerTest, StoppingIdleDispatcherWakesItWithoutADeviceEvent)
{
    EventListenerState state;
    state.activate();
    std::promise<void> stopped;
    auto completion = stopped.get_future();
    DispatcherThread dispatcher(state, [&] {
        state.run(
            {}, [](const std::wstring &) { return "COM3"; },
            [](PortEvent, const char *) { FAIL() << "No event was queued"; },
            [](const char *message) { FAIL() << message; });
        stopped.set_value();
    });
    state.stop();
    EXPECT_EQ(completion.wait_for(std::chrono::seconds(2)), std::future_status::ready);
}

TEST(SerialEventListenerTest, ResolutionErrorsDoNotPreventLaterEventsOrReentrantStop)
{
    EventListenerState state;
    state.enqueue(PortEvent::kAttached, L"unavailable");
    state.enqueue(PortEvent::kDetached, L"unavailable");
    state.enqueue(PortEvent::kAttached, L"available");
    state.activate();

    int errors = 0;
    int callbacks = 0;
    state.run(
        {},
        [](const std::wstring &path) -> std::string {
            if (path == L"unavailable")
            {
                throw std::runtime_error("Device disappeared");
            }
            return "COM5";
        },
        [&](PortEvent event, const char *port) {
            EXPECT_EQ(event, PortEvent::kAttached);
            EXPECT_STREQ(port, "COM5");
            ++callbacks;
            state.stop();
        },
        [&](const char *message) {
            EXPECT_STREQ(message, "Device disappeared");
            ++errors;
        });
    EXPECT_EQ(errors, 1);
    EXPECT_EQ(callbacks, 1);
}

TEST(SerialEventListenerTest, QueueFailureIsReportedOnDispatcherAndErrorCallbackCanStop)
{
    EventListenerState state;
    state.reportQueueFailure();
    state.activate();
    int errors = 0;
    state.run(
        {}, [](const std::wstring &) { return "COM3"; },
        [](PortEvent, const char *) { FAIL() << "No event was queued"; },
        [&](const char *) {
            ++errors;
            state.stop();
        });
    EXPECT_EQ(errors, 1);
}

TEST(SerialEventListenerTest, ConcurrentProducersWakeDispatcherAndPreserveEachDevicesEventOrder)
{
    EventListenerState state;
    state.activate();
    constexpr std::size_t kDevices = 64;
    std::promise<void> delivered;
    auto completion = delivered.get_future();
    std::vector<Event> events;
    {
        DispatcherThread dispatcher(state, [&] {
            state.run(
                {}, [](const std::wstring &path) { return std::string(path.begin(), path.end()); },
                [&](PortEvent event, const char *port) {
                    events.emplace_back(event, port);
                    if (events.size() == kDevices * 2)
                    {
                        state.stop();
                        delivered.set_value();
                    }
                },
                [](const char *message) { FAIL() << message; });
        });
        const auto produce = [&](std::size_t first) {
            for (std::size_t index = first; index < kDevices; index += 2)
            {
                const auto path = L"COM" + std::to_wstring(index);
                state.enqueue(PortEvent::kAttached, path);
                state.enqueue(PortEvent::kDetached, path);
            }
        };
        std::thread first(produce, 0);
        std::thread second(produce, 1);
        first.join();
        second.join();
        EXPECT_EQ(completion.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    }
    ASSERT_EQ(events.size(), kDevices * 2);
    std::map<std::string, int> counts;
    for (const auto &[event, port] : events)
    {
        auto &count = counts[port];
        EXPECT_EQ(event, count == 0 ? PortEvent::kAttached : PortEvent::kDetached);
        ++count;
    }
    EXPECT_EQ(counts.size(), kDevices);
    for (const auto &[port, count] : counts)
    {
        EXPECT_EQ(count, 2) << port;
    }
}

namespace
{
void portEvent(cpp_core::PortEvent, const char *)
{
}
} // namespace

TEST(SerialSetEventCallbackTest, EventCallbackCanBeReplacedAndCleared)
{
    EXPECT_EQ(serialSetEventCallback(nullptr), 0);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(serialSetEventCallback(portEvent), 0);
        EXPECT_EQ(serialSetEventCallback(portEvent), 0);
        EXPECT_EQ(serialSetEventCallback(nullptr), 0);
    }
}
