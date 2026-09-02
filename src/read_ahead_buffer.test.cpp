#include "detail/read_ahead_buffer.hpp"
#include "detail/copy_until_terminator.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

namespace
{
using cpp_bindings_windows::detail::copyUntilTerminator;
using cpp_bindings_windows::detail::ReadAheadBuffer;
} // namespace

TEST(CopyUntilTerminatorTest, StopsExactlyAfterTerminatorInLongChunk)
{
    std::string input(1000, 'A');
    input += "\ntail";
    std::vector<unsigned char> output(input.size());
    constexpr unsigned char newline = '\n';

    const auto result = copyUntilTerminator(output.data(), 0, reinterpret_cast<const unsigned char *>(input.data()),
                                            static_cast<int>(input.size()), &newline, 1);

    ASSERT_TRUE(result.terminator_found);
    ASSERT_EQ(result.bytes_copied, 1001);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char *>(output.data()), result.bytes_copied),
              std::string_view(input.data(), 1001));
}

TEST(CopyUntilTerminatorTest, FindsSequenceAcrossChunkBoundary)
{
    std::array<unsigned char, 16> output{};
    constexpr std::string_view prefix = "prefix-EN";
    constexpr std::string_view input = "D-tail";
    constexpr unsigned char terminator[] = {'E', 'N', 'D'};
    std::copy(prefix.begin(), prefix.end(), output.begin());

    const auto result = copyUntilTerminator(
        output.data(), static_cast<int>(prefix.size()), reinterpret_cast<const unsigned char *>(input.data()),
        static_cast<int>(input.size()), terminator, static_cast<int>(std::size(terminator)));

    ASSERT_TRUE(result.terminator_found);
    ASSERT_EQ(result.bytes_copied, 1);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char *>(output.data()), prefix.size() + result.bytes_copied),
              "prefix-END");
}

TEST(ReadAheadBufferTest, PreservesBytesAfterTerminatorForNextRead)
{
    ReadAheadBuffer buffer;
    constexpr std::string_view input = "line\nnext";
    buffer.append(reinterpret_cast<const unsigned char *>(input.data()), static_cast<int>(input.size()));

    std::array<unsigned char, 16> first_output{};
    constexpr unsigned char newline = '\n';
    const auto first = buffer.consume(first_output.data(), 0, static_cast<int>(first_output.size()), &newline, 1);

    ASSERT_TRUE(first.terminator_found);
    ASSERT_EQ(first.bytes_copied, 5);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char *>(first_output.data()), first.bytes_copied), "line\n");
    ASSERT_EQ(buffer.size(), 4);

    std::array<unsigned char, 4> second_output{};
    const auto second = buffer.consume(second_output.data(), 0, static_cast<int>(second_output.size()), nullptr, 0);

    EXPECT_FALSE(second.terminator_found);
    ASSERT_EQ(second.bytes_copied, 4);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char *>(second_output.data()), second.bytes_copied), "next");
    EXPECT_EQ(buffer.size(), 0);
}

TEST(ReadAheadBufferTest, ClearDropsBufferedBytes)
{
    ReadAheadBuffer buffer;
    constexpr std::array<unsigned char, 3> input = {'a', 'b', 'c'};
    buffer.append(input.data(), static_cast<int>(input.size()));

    buffer.clear();

    EXPECT_EQ(buffer.size(), 0);
}
