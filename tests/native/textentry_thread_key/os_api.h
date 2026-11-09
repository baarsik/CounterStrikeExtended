#ifndef CSE_TEST_TEXTENTRY_KEY_OS_API_H
#define CSE_TEST_TEXTENTRY_KEY_OS_API_H

#include <cstdint>

enum class KeyFault : std::uint32_t {
    None,
    CreateAgain,
    CreateNoMemory,
    SetNoMemory,
    RestoreNoMemory,
    DeleteInvalid,
    LockBusy,
    DestructorLockBusy,
    UnlockPermission,
    DestroyBusy
};

struct KeyOsStats {
    std::uint64_t creates;
    std::uint64_t created;
    std::uint64_t gets;
    std::uint64_t sets;
    std::uint64_t deletes;
    std::uint64_t deleted;
    std::uint64_t locks;
    std::uint64_t unlocks;
    std::uint64_t destroys;
    std::uint64_t destroyed;
    std::uint64_t nonnullDestructors;
    bool keyOwned;
    bool zeroKeyMode;
};

// Test-only OS boundary control. The real class/Buffer/widget methods are never
// replaced. Positive paths call the actual synchronous libSystem pthread API.
void SetKeyFault(KeyFault fault);
void SetZeroKeyMode(bool zero);
KeyOsStats ReadKeyOsStats();

#endif
