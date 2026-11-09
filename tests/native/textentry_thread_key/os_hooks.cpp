#include "os_api.h"

#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <dlfcn.h>
#include <pthread.h>

namespace {
std::atomic<std::uint64_t> Creates{0};
std::atomic<std::uint64_t> Created{0};
std::atomic<std::uint64_t> Gets{0};
std::atomic<std::uint64_t> Sets{0};
std::atomic<std::uint64_t> Deletes{0};
std::atomic<std::uint64_t> Deleted{0};
std::atomic<std::uint64_t> Locks{0};
std::atomic<std::uint64_t> Unlocks{0};
std::atomic<std::uint64_t> Destroys{0};
std::atomic<std::uint64_t> Destroyed{0};
std::atomic<std::uint64_t> NonnullDestructors{0};
KeyFault Fault = KeyFault::None;
bool ZeroKey = false;
bool Owned = false;
pthread_key_t PhysicalKey{};
pthread_key_t LogicalKey{};

template<class Function>
Function Real(const char* name) {
    void* symbol = dlsym(RTLD_NEXT, name);
    if (!symbol) {
        std::abort();
    }
    // These are ordinary POSIX C function symbols, never C++ member pointers.
    return reinterpret_cast<Function>(symbol);
}

pthread_key_t Physical(pthread_key_t logical) {
    if (!Owned || logical != LogicalKey) {
        std::fputs("controlled OS trap: invalid or deleted key\n", stderr);
        std::abort();
    }
    return PhysicalKey;
}
}

void SetKeyFault(KeyFault fault) {
    Fault = fault;
}

void SetZeroKeyMode(bool zero) {
    if (Owned) {
        std::fputs("controlled OS trap: duplicate owned key creation\n", stderr);
        std::abort();
    }
    ZeroKey = zero;
}

KeyOsStats ReadKeyOsStats() {
    return {Creates.load(), Created.load(), Gets.load(), Sets.load(), Deletes.load(), Deleted.load(),
        Locks.load(), Unlocks.load(), Destroys.load(), Destroyed.load(), NonnullDestructors.load(), Owned, ZeroKey};
}

// The module linker makes these seven test definitions private so they cannot
// interpose on another image's key or on the host's std::thread implementation.
extern "C" int pthread_key_create(pthread_key_t* key, void (*destructor)(void*)) {
    ++Creates;
    if (destructor) {
        ++NonnullDestructors;
    }
    if (Fault == KeyFault::CreateAgain) {
        return EAGAIN;
    }
    if (Fault == KeyFault::CreateNoMemory) {
        return ENOMEM;
    }
    if (Owned) {
        std::abort();
    }
    const int result = Real<decltype(&pthread_key_create)>("pthread_key_create")(&PhysicalKey, destructor);
    if (result == 0) {
        LogicalKey = ZeroKey ? pthread_key_t{} : PhysicalKey;
        *key = LogicalKey;
        Owned = true;
        ++Created;
    }
    return result;
}

extern "C" void* pthread_getspecific(pthread_key_t key) {
    ++Gets;
    return Real<decltype(&pthread_getspecific)>("pthread_getspecific")(Physical(key));
}

extern "C" int pthread_setspecific(pthread_key_t key, const void* value) {
    ++Sets;
    if (Fault == KeyFault::SetNoMemory || (Fault == KeyFault::RestoreNoMemory && !value)) {
        return ENOMEM;
    }
    return Real<decltype(&pthread_setspecific)>("pthread_setspecific")(Physical(key), value);
}

extern "C" int pthread_key_delete(pthread_key_t key) {
    ++Deletes;
    if (Fault == KeyFault::DeleteInvalid) {
        return EINVAL;
    }
    const int result = Real<decltype(&pthread_key_delete)>("pthread_key_delete")(Physical(key));
    if (result == 0) {
        Owned = false;
        ++Deleted;
    }
    return result;
}

extern "C" int pthread_mutex_lock(pthread_mutex_t* mutex) {
    if (Destroyed.load() != 0) {
        std::fputs("controlled OS trap: access to destroyed mutex\n", stderr);
        std::abort();
    }
    const auto call = ++Locks;
    if (Fault == KeyFault::LockBusy || (Fault == KeyFault::DestructorLockBusy && call == 2)) {
        return EBUSY;
    }
    return Real<decltype(&pthread_mutex_lock)>("pthread_mutex_lock")(mutex);
}

extern "C" int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    if (Destroyed.load() != 0) {
        std::fputs("controlled OS trap: access to destroyed mutex\n", stderr);
        std::abort();
    }
    ++Unlocks;
    if (Fault == KeyFault::UnlockPermission) {
        return EPERM;
    }
    return Real<decltype(&pthread_mutex_unlock)>("pthread_mutex_unlock")(mutex);
}

extern "C" int pthread_mutex_destroy(pthread_mutex_t* mutex) {
    if (Destroyed.load() != 0) {
        std::fputs("controlled OS trap: repeated mutex destruction\n", stderr);
        std::abort();
    }
    ++Destroys;
    if (Fault == KeyFault::DestroyBusy) {
        return EBUSY;
    }
    const int result = Real<decltype(&pthread_mutex_destroy)>("pthread_mutex_destroy")(mutex);
    if (result == 0) {
        ++Destroyed;
    }
    return result;
}
