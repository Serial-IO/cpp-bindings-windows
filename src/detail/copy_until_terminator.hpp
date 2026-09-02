#pragma once

#include "matches_suffix.hpp"

namespace cpp_bindings_windows::detail
{
struct CopyUntilTerminatorResult
{
    int bytes_copied = 0;
    bool terminator_found = false;
};

inline auto copyUntilTerminator(unsigned char *output, int output_size, const unsigned char *input, int input_size,
                                const unsigned char *terminator, int terminator_size) -> CopyUntilTerminatorResult
{
    CopyUntilTerminatorResult result;
    while (result.bytes_copied < input_size)
    {
        output[output_size + result.bytes_copied] = input[result.bytes_copied];
        ++result.bytes_copied;

        if (matchesSuffix(output, output_size + result.bytes_copied, terminator, terminator_size))
        {
            result.terminator_found = true;
            break;
        }
    }
    return result;
}
} // namespace cpp_bindings_windows::detail
