#pragma once
#include "platform_types.h"
#include <cstddef>
#include <cstdint>
#ifdef _WIN32
#include <mmsystem.h>
inline std::uint32_t A3dMonotonicMilliseconds() noexcept {
    return timeGetTime();
}
inline std::uint32_t A3dTickMilliseconds() noexcept {
    return GetTickCount();
}
inline void* A3dAllocate(std::size_t size) noexcept {
    return GlobalAlloc(GMEM_FIXED, size);
}
inline void A3dFree(void* data) noexcept {
    GlobalFree(data);
}
inline LONG A3dReferenceIncrement(LONG* value) noexcept {
    return InterlockedIncrement(value);
}
inline LONG A3dReferenceDecrement(LONG* value) noexcept {
    return InterlockedDecrement(value);
}
#else
#include <array>
enum class A3dNativeOwnerKind : std::size_t {
    Provider, Leaf, Parent, Source, ResourceManager, StreamBuffer, DirectBuffer,
    Samples, Pool, PoolBuffer, Output, OutputWorker, ResourceWorker, Callback, Count
};
using A3dNativeOwnerCounts = std::array<std::size_t, static_cast<std::size_t>(A3dNativeOwnerKind::Count)>;
void A3dNativeTrackOwner(A3dNativeOwnerKind kind, int delta) noexcept;
A3dNativeOwnerCounts A3dNativeOwners() noexcept;
class A3dNativeOwnerLease {
public:
    explicit A3dNativeOwnerLease(A3dNativeOwnerKind kind) noexcept : kind_(kind) {
        A3dNativeTrackOwner(kind_, 1);
    }
    ~A3dNativeOwnerLease() {
        A3dNativeTrackOwner(kind_, -1);
    }
    A3dNativeOwnerLease(const A3dNativeOwnerLease&) = delete;
    A3dNativeOwnerLease& operator=(const A3dNativeOwnerLease&) = delete;
private:
    A3dNativeOwnerKind kind_;
};
#ifdef A3D_EVENT_TESTING
void A3dNativeFailAcquisitionAfter(LONG acquisitions) noexcept;
#endif
bool A3dNativeAcquisitionSucceeded() noexcept;

struct SDL_Thread;
struct SDL_Mutex;
struct SDL_Condition;
std::uint32_t A3dMonotonicMilliseconds() noexcept;
inline std::uint32_t A3dTickMilliseconds() noexcept {
    return A3dMonotonicMilliseconds();
}
void* A3dAllocate(std::size_t size) noexcept;
void A3dFree(void* data) noexcept;
inline LONG A3dReferenceIncrement(LONG* value) noexcept {
    return __atomic_add_fetch(value, 1, __ATOMIC_SEQ_CST);
}
inline LONG A3dReferenceDecrement(LONG* value) noexcept {
    return __atomic_sub_fetch(value, 1, __ATOMIC_SEQ_CST);
}

// Internal periodic worker, not a Windows HANDLE or an external notification event.
// Start/Stop are serialized by the owner; Stop must never be called from its worker.
class A3dNativeWorker {
public:
    using Function = DWORD (*)(void*);
    A3dNativeWorker() = default;
    ~A3dNativeWorker();
    A3dNativeWorker(const A3dNativeWorker&) = delete;
    A3dNativeWorker& operator=(const A3dNativeWorker&) = delete;
    HRESULT Start(Function function, void* context) noexcept;
    bool Wait(DWORD milliseconds) noexcept;
    void Stop() noexcept;
private:
    static int Run(void* context);
    SDL_Thread* thread_ = nullptr;
    SDL_Mutex* mutex_ = nullptr;
    SDL_Condition* condition_ = nullptr;
    Function function_ = nullptr;
    void* context_ = nullptr;
    bool stopping_ = false;
};

class A3dNativeWave {
public:
    using Allocate = void* (*)(std::size_t);
    using Free = void (*)(void*);
    // Matched allocation operations permit explicit allocation-failure injection.
    explicit A3dNativeWave(Allocate allocate = A3dAllocate, Free free = A3dFree) noexcept
        : allocate_(allocate), free_(free) {}
    ~A3dNativeWave();
    A3dNativeWave(const A3dNativeWave&) = delete;
    A3dNativeWave& operator=(const A3dNativeWave&) = delete;
    HRESULT Load(const char* filename) noexcept;
    const BYTE* Data() const noexcept {
        return data_;
    }
    DWORD Size() const noexcept {
        return size_;
    }
    const PCMWAVEFORMAT& Format() const noexcept {
        return format_;
    }
private:
    Allocate allocate_;
    Free free_;
    BYTE* data_ = nullptr;
    DWORD size_ = 0;
    PCMWAVEFORMAT format_{};
};
#endif
