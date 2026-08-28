#pragma once

#include "common_types.hpp"
#include "windows.hpp"

#include <cpp_core/unique_resource.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace cpp_bindings_windows::detail
{
enum class Operation
{
    kRead,
    kWrite,
};

struct Win32HandleTraits
{
    using handle_type = HANDLE; // NOLINT(readability-identifier-naming)

    static constexpr auto invalid() noexcept -> handle_type
    {
        return nullptr;
    }

    static auto close(handle_type handle) noexcept -> void
    {
        if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle(handle);
        }
    }
};

using UniqueHandle = cpp_core::UniqueResource<Win32HandleTraits>;

struct HandleState
{
    std::atomic<int64_t> bytes_read_total{0};
    std::atomic<int64_t> bytes_written_total{0};
    std::atomic<bool> abort_read{false};
    std::atomic<bool> abort_write{false};
    std::mutex pending_io_mutex;
    OVERLAPPED *pending_read = nullptr;
    OVERLAPPED *pending_write = nullptr;
};

struct HandleContext
{
    HANDLE handle = nullptr;
    std::shared_ptr<HandleState> state;
};

struct PendingIoStart
{
    BOOL completed = FALSE;
    DWORD error = ERROR_SUCCESS;
    bool aborted = false;
};

inline std::mutex g_handle_states_mutex;
inline std::unordered_map<std::uintptr_t, std::shared_ptr<HandleState>> g_handle_states;
inline std::atomic<IoCallbackT> g_read_callback{nullptr};
inline std::atomic<IoCallbackT> g_write_callback{nullptr};
} // namespace cpp_bindings_windows::detail
