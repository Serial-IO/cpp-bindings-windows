// Load the binding's generated version before cpp-core's identically guarded header.
#include "version.hpp"

#include <cpp_core/interface/meta.h>

MODULE_API void meta(cpp_core::Meta *out)
{
    if (out != nullptr)
    {
        *out = cpp_core::Meta{
            .major = version::MAJOR,
            .minor = version::MINOR,
            .patch = version::PATCH,
            .commits_since_tag = version::GIT_COMMIT_COUNT,
            .is_dirty = version::GIT_IS_DIRTY ? 1 : 0,
            .version_string = version::VERSION,
            .prerelease = version::PRERELEASE,
            .prerelease_type = version::PRERELEASE_TYPE,
            .prerelease_number = version::PRERELEASE_NUMBER,
            .git_tag = version::GIT_TAG,
            .git_describe_hash = version::GIT_DESCRIBE_HASH,
            .git_commit_hash_short = version::GIT_COMMIT_HASH_SHORT,
            .git_commit_hash_full = version::GIT_COMMIT_HASH_FULL,
            .git_commit_date = version::GIT_COMMIT_DATE,
            .git_branch = version::GIT_BRANCH,
            .git_dirty_suffix = version::GIT_DIRTY_SUFFIX,
        };
    }
}
