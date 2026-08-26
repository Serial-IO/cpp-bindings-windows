#pragma once

#include "common_types.hpp"

#include <cpp_core/error_handling.hpp>
#include <cpp_core/unique_resource.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

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

inline auto handleKey(HANDLE handle) -> std::uintptr_t
{
    return reinterpret_cast<std::uintptr_t>(handle);
}

inline auto effectiveErrorCallback(ErrorCallbackT error_callback) -> ErrorCallbackT
{
    return error_callback != nullptr ? error_callback : g_error_callback.load(std::memory_order_acquire);
}

inline auto ensureHandleState(HANDLE handle) -> std::shared_ptr<HandleState>
{
    std::lock_guard lock(g_handle_states_mutex);
    auto &state = g_handle_states[handleKey(handle)];
    if (!state)
    {
        state = std::make_shared<HandleState>();
    }
    return state;
}

inline auto registerOpenedHandle(HANDLE handle) -> void
{
    (void)ensureHandleState(handle);
}

inline auto removeHandleState(HANDLE handle) -> void
{
    std::lock_guard lock(g_handle_states_mutex);
    g_handle_states.erase(handleKey(handle));
}

template <cpp_core::StatusConvertible ReturnType>
inline auto validateWin32Handle(int64_t handle, ErrorCallbackT error_callback, HANDLE *out_handle) -> ReturnType
{
    const auto callback = effectiveErrorCallback(error_callback);
    if (handle <= 0)
    {
        return cpp_core::failMsg<ReturnType>(
            callback, static_cast<StatusCodeValue>(StatusCode::Connection::kInvalidHandleError), "Invalid handle");
    }

    if constexpr (sizeof(intptr_t) < sizeof(int64_t))
    {
        if (handle > static_cast<int64_t>(std::numeric_limits<intptr_t>::max()))
        {
            return cpp_core::failMsg<ReturnType>(
                callback, static_cast<StatusCodeValue>(StatusCode::Connection::kInvalidHandleError), "Invalid handle");
        }
    }

    const auto native_handle = reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
    if (native_handle == nullptr || native_handle == INVALID_HANDLE_VALUE)
    {
        return cpp_core::failMsg<ReturnType>(
            callback, static_cast<StatusCodeValue>(StatusCode::Connection::kInvalidHandleError), "Invalid handle");
    }

    *out_handle = native_handle;
    return static_cast<ReturnType>(StatusCode::kSuccess);
}

template <cpp_core::StatusConvertible ReturnType>
inline auto acquireHandleContext(int64_t handle, ErrorCallbackT error_callback, HandleContext *out_context)
    -> ReturnType
{
    HANDLE native_handle = nullptr;
    const auto status = validateWin32Handle<ReturnType>(handle, error_callback, &native_handle);
    if (status < 0)
    {
        return status;
    }

    out_context->handle = native_handle;
    out_context->state = ensureHandleState(native_handle);
    return static_cast<ReturnType>(StatusCode::kSuccess);
}

inline auto abortFlag(const std::shared_ptr<HandleState> &state, Operation operation) -> std::atomic<bool> &
{
    return operation == Operation::kRead ? state->abort_read : state->abort_write;
}

inline auto pendingOperation(const std::shared_ptr<HandleState> &state, Operation operation) -> OVERLAPPED *&
{
    return operation == Operation::kRead ? state->pending_read : state->pending_write;
}

inline auto requestAbort(HANDLE handle, const std::shared_ptr<HandleState> &state, Operation operation) -> void
{
    abortFlag(state, operation).store(true, std::memory_order_release);

    std::lock_guard lock(state->pending_io_mutex);
    if (auto *pending = pendingOperation(state, operation); pending != nullptr)
    {
        (void)CancelIoEx(handle, pending);
    }
}

inline auto consumeAbort(const std::shared_ptr<HandleState> &state, Operation operation) -> bool
{
    return abortFlag(state, operation).exchange(false, std::memory_order_acq_rel);
}

template <typename StartOperation>
inline auto startPendingIo(const std::shared_ptr<HandleState> &state, Operation operation, OVERLAPPED *overlapped,
                           StartOperation &&start_operation) -> PendingIoStart
{
    std::lock_guard lock(state->pending_io_mutex);
    if (consumeAbort(state, operation))
    {
        return {.aborted = true};
    }

    pendingOperation(state, operation) = overlapped;
    const BOOL completed = std::forward<StartOperation>(start_operation)();
    return {.completed = completed, .error = completed != FALSE ? ERROR_SUCCESS : GetLastError()};
}

inline auto finishPendingIo(const std::shared_ptr<HandleState> &state, Operation operation, OVERLAPPED *overlapped)
    -> bool
{
    std::lock_guard lock(state->pending_io_mutex);
    auto &pending = pendingOperation(state, operation);
    if (pending == overlapped)
    {
        pending = nullptr;
    }
    return consumeAbort(state, operation);
}

inline auto noteBytesTransferred(const std::shared_ptr<HandleState> &state, Operation operation, int transferred_bytes)
    -> void
{
    if (operation == Operation::kRead)
    {
        state->bytes_read_total.fetch_add(transferred_bytes, std::memory_order_relaxed);
        if (const auto callback = g_read_callback.load(std::memory_order_acquire); callback != nullptr)
        {
            callback(transferred_bytes);
        }
        return;
    }

    state->bytes_written_total.fetch_add(transferred_bytes, std::memory_order_relaxed);
    if (const auto callback = g_write_callback.load(std::memory_order_acquire); callback != nullptr)
    {
        callback(transferred_bytes);
    }
}

} // namespace cpp_bindings_windows::detail
