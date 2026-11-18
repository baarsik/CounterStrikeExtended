#ifndef CSE_FILESYSTEM_LOCALIZED_FILESYSTEM_H
#define CSE_FILESYSTEM_LOCALIZED_FILESYSTEM_H

#include <cstdint>

namespace cse {
namespace localized_filesystem {
std::uint32_t CurrentGroup();
void RecordFollowingMount(const char* root, const char* pathId);
void ResetLocalizedFileSystem();
} // namespace localized_filesystem
} // namespace cse

#endif
