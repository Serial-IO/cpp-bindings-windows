#include <cpp_core/interface/meta.h>

#include <cstring>
#include <string_view>

#include <gtest/gtest.h>

TEST(MetaTest, MetadataDescribesLoadedBinding)
{
    meta(nullptr);
    cpp_core::Meta info{};
    meta(&info);
    EXPECT_STREQ(info.version_string, CPP_BINDINGS_WINDOWS_TEST_VERSION);
    ASSERT_NE(info.git_commit_hash_full, nullptr);
    EXPECT_EQ(std::strlen(info.git_commit_hash_full), 40U);
    ASSERT_NE(info.git_commit_hash_short, nullptr);
    EXPECT_TRUE(std::string_view(info.git_commit_hash_full).starts_with(info.git_commit_hash_short));
    EXPECT_NE(info.prerelease, nullptr);
    EXPECT_NE(info.git_tag, nullptr);
    EXPECT_NE(info.git_commit_date, nullptr);
    EXPECT_NE(info.git_branch, nullptr);
}
