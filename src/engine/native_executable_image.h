#pragma once

#include <dlfcn.h>
#include <mach-o/loader.h>

namespace cse {

inline const void* MainExecutableImage(const void* address) noexcept {
    Dl_info image{};
    if (!address || !dladdr(address, &image) || !image.dli_fbase) {
        return nullptr;
    }
    const auto* header = static_cast<const mach_header_64*>(image.dli_fbase);
    // dyld can enumerate a preloaded library before the main executable.
    return header->magic == MH_MAGIC_64 && header->filetype == MH_EXECUTE ? header : nullptr;
}

} // namespace cse
