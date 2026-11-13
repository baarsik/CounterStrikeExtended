#pragma once

#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <set>
#include <vector>

// This implementation must live in one shared dylib. Independent copies cannot
// reference-count CoreText's process-wide catalog safely.
extern "C" {
__attribute__((visibility("default"))) int CseMacFontRegistrationAcquire(
    const void* owner, const char* resolvedUtf8Path) noexcept;
__attribute__((visibility("default"))) int CseMacFontRegistrationReleaseOwner(const void* owner) noexcept;
}

namespace cse::fonts {

namespace {

// Internal linkage gives each consuming translation unit its own unload guard.
// The linked shared service remains loaded until these client destructors finish.
class MacFontLeaseBook {
public:
    ~MacFontLeaseBook() {
        for (const void* owner : owners_) {
            if (!CseMacFontRegistrationReleaseOwner(owner)) {
                std::fprintf(stderr, "Unable to release macOS custom fonts during module unload\n");
            }
        }
    }

    bool Acquire(const void* owner, const char* path) {
        owners_.insert(owner);
        return CseMacFontRegistrationAcquire(owner, path) != 0;
    }

    bool Release(const void* owner) {
        if (!CseMacFontRegistrationReleaseOwner(owner)) {
            std::fprintf(stderr, "Unable to release macOS custom fonts during Surface shutdown\n");
            return false;
        }
        owners_.erase(owner);
        return true;
    }

private:
    std::set<const void*> owners_;
};

MacFontLeaseBook& MacFontLeases() {
    static MacFontLeaseBook leases;
    return leases;
}

}

// GetLocalPath can silently truncate and returns no required-length value.
// Retry near-full results, including the pack-path snprintf(size - 1) case.
template <typename FileSystem> static bool RegisterMacCustomFontFile(
    const void* owner, FileSystem* filesystem, const char* filename) {
    if (!owner || !filesystem || !filename || !filename[0]) {
        return false;
    }
    constexpr std::size_t initialPathCapacity = 512;
    constexpr std::size_t truncationMargin = 2;
    constexpr std::size_t growthFactor = 2;
    std::vector<char> path(initialPathCapacity);
    for (;;) {
        std::fill(path.begin(), path.end(), '\0');
        const char* resolved = filesystem->GetLocalPath(filename, path.data(), static_cast<int>(path.size()));
        if (!resolved) {
            return false;
        }
        const std::size_t length = strnlen(resolved, path.size());
        if (length == 0) {
            return false;
        }
        if (length < path.size() - truncationMargin) {
            return MacFontLeases().Acquire(owner, resolved);
        }
        constexpr std::size_t maxPathCapacity = INT_MAX;
        if (path.size() > maxPathCapacity / growthFactor) {
            return false;
        }
        path.resize(path.size() * growthFactor);
    }
}

// Surface's void Shutdown reports failures and leaves its lease tracked for a
// subsequent shutdown/module-unload retry. A successful release is idempotent.
static inline bool ReleaseMacCustomFonts(const void* owner) {
    return MacFontLeases().Release(owner);
}

}
