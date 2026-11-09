#ifndef CSE_NATIVE_ENGINE_MEMORY_H
#define CSE_NATIVE_ENGINE_MEMORY_H

#include "legacy_text_to_integer.h"

#include <cstdint>
#include <cstdlib>
#include <limits>

#if defined(__APPLE__) && !defined(_WIN32)
#include <unistd.h>
#endif

namespace cse_native_engine_memory {
constexpr int kBytesPerKibibyte = 1024;
constexpr int kBytesPerMebibyte = kBytesPerKibibyte * kBytesPerKibibyte;
constexpr int kMinimumHeapBytes = 14 * kBytesPerMebibyte;
constexpr int kDedicatedHeapBytes = 40 * kBytesPerMebibyte;
constexpr int kMaximumHeapBytes = 128 * kBytesPerMebibyte;
constexpr std::uint64_t kMinimumPhysicalBytes = 15 * kBytesPerMebibyte;

static_assert(std::numeric_limits<int>::digits == 31, "The retained host heap field is signed32");

enum class QueryStatus {
    Known,
    InvalidPageCount,
    InvalidPageSize,
    ProductOverflow
};

struct PhysicalMemory {
    QueryStatus status;
    std::uint64_t totalBytes;
};

struct HeapPlan {
    int bytes;
    bool usedUnknownFallback;
};

enum class PlanStatus {
    Ready,
    PhysicalMemoryTooSmall
};

// Keep the Win32 decimal prefix rules, but reject requests whose legacy shift
// would have been negative or outside the signed32 host field. Failure is transactional.
inline bool ParseHeapArgument(const char* argument, int& bytes) noexcept {
    if (argument == nullptr) {
        return false;
    }
    const std::int32_t kibibytes = CseLegacyAtoi(argument);
    constexpr int kMaximumKibibytes = (std::numeric_limits<int>::max)() / kBytesPerKibibyte;
    if (kibibytes < 0 || kibibytes > kMaximumKibibytes) {
        return false;
    }
    bytes = kibibytes * kBytesPerKibibyte;
    return true;
}

inline PhysicalMemory ClassifyPhysicalMemory(std::int64_t pages, std::int64_t pageBytes) noexcept {
    if (pages <= 0) {
        return {QueryStatus::InvalidPageCount, 0};
    }
    if (pageBytes <= 0) {
        return {QueryStatus::InvalidPageSize, 0};
    }
    const std::uint64_t count = static_cast<std::uint64_t>(pages);
    const std::uint64_t size = static_cast<std::uint64_t>(pageBytes);
    if (count > (std::numeric_limits<std::uint64_t>::max)() / size) {
        return {QueryStatus::ProductOverflow, 0};
    }
    return {QueryStatus::Known, count * size};
}

#if defined(__APPLE__) && !defined(_WIN32)
inline PhysicalMemory QueryPhysicalMemory() noexcept {
    // Use the actual SDK names and full-width return values. Both queries retain
    // their original order even when the first result is unavailable.
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long pageBytes = ::sysconf(_SC_PAGESIZE);
    return ClassifyPhysicalMemory(pages, pageBytes);
}
#endif

inline PlanStatus PlanHeap(int configuredBytes, PhysicalMemory physical, bool dedicated, HeapPlan& plan) noexcept {
    int bytes = configuredBytes;
    const bool automatic = bytes < kMinimumHeapBytes;
    const bool unknown = automatic && physical.status != QueryStatus::Known;
    if (automatic) {
        // Unknown is explicitly the original Windows dwTotalPhys == 0 fallback,
        // not a fabricated measurement. Known low RAM must fail before overrides.
        if (!unknown && physical.totalBytes < kMinimumPhysicalBytes) {
            return PlanStatus::PhysicalMemoryTooSmall;
        }
        constexpr std::uint64_t kMaximumLegacyPhysicalBytes = 0x7fffffff;
        if (unknown || physical.totalBytes > kMaximumLegacyPhysicalBytes) {
            bytes = kMaximumHeapBytes;
        } else {
            bytes = static_cast<int>(physical.totalBytes / 2);
            if (bytes < kMinimumHeapBytes) {
                bytes = kMinimumHeapBytes;
            }
        }
        if (dedicated) {
            bytes = kDedicatedHeapBytes;
        }
    }
    if (bytes > kMaximumHeapBytes) {
        bytes = kMaximumHeapBytes;
    }
    plan = {bytes, unknown};
    return PlanStatus::Ready;
}

inline void* AllocateHeap(int bytes) noexcept {
    if (bytes < kMinimumHeapBytes || bytes > kMaximumHeapBytes) {
        return nullptr;
    }
    return std::malloc(static_cast<std::size_t>(bytes));
}

inline void ReleaseHeap(void*& base, int& bytes) noexcept {
    std::free(base);
    base = nullptr;
    bytes = 0;
}
}

#endif
