#pragma once

#include "copy_until_terminator.hpp"

#include <climits>
#include <cstddef>
#include <deque>
#include <mutex>

namespace cpp_bindings_windows::detail
{
class ReadAheadBuffer
{
  public:
    auto append(const unsigned char *data, int data_size) -> void
    {
        if (data == nullptr || data_size <= 0)
        {
            return;
        }

        std::lock_guard lock(mutex_);
        bytes_.insert(bytes_.end(), data, data + data_size);
    }

    auto consume(unsigned char *output, int output_size, int max_bytes, const unsigned char *terminator,
                 int terminator_size) -> CopyUntilTerminatorResult
    {
        CopyUntilTerminatorResult result;
        if (output == nullptr || max_bytes <= 0)
        {
            return result;
        }

        std::lock_guard lock(mutex_);
        while (result.bytes_copied < max_bytes && !bytes_.empty())
        {
            output[output_size + result.bytes_copied] = bytes_.front();
            bytes_.pop_front();
            ++result.bytes_copied;

            if (matchesSuffix(output, output_size + result.bytes_copied, terminator, terminator_size))
            {
                result.terminator_found = true;
                break;
            }
        }
        return result;
    }

    auto clear() -> void
    {
        std::lock_guard lock(mutex_);
        bytes_.clear();
    }

    [[nodiscard]] auto size() const -> int
    {
        std::lock_guard lock(mutex_);
        return bytes_.size() > static_cast<std::size_t>(INT_MAX) ? INT_MAX : static_cast<int>(bytes_.size());
    }

  private:
    mutable std::mutex mutex_;
    std::deque<unsigned char> bytes_;
};
} // namespace cpp_bindings_windows::detail
