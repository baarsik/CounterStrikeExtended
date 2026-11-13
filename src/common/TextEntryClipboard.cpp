#include "TextEntryClipboard.h"

#include <climits>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>

#if defined(__APPLE__) && !defined(_WIN32) && defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) \
    && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1
#include <atomic>
#include <cstdio>
#include <pthread.h>
#endif

namespace cse_textentry_clipboard {
namespace {

// The existing clipboard getter accepts a signed byte count. Keep all caller
// storage and growth inside that physical bound, with a separate spare NUL cell.
constexpr int MaximumCapacity = INT_MAX / static_cast<int>(sizeof(wchar_t));

#if defined(__APPLE__) && !defined(_WIN32) && defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) \
    && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1

constexpr std::uint32_t QuiescenceVersion = 1;

enum class RegistryPhase : std::uint32_t {
    Open,
    KeyRetirementPending,
    MutexRetirementPending,
    Retired
};

struct NativeRegistry {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_key_t key{};
    bool keyCreated = false;
    std::uint64_t activeFrames = 0;
    std::atomic<RegistryPhase> phase{RegistryPhase::Open};
};

static_assert(ATOMIC_INT_LOCK_FREE == 2, "Apple registry phase must not introduce hidden lock ownership");
NativeRegistry nativeRegistry;

void ReportNativeFailure(const char* operation, int error) noexcept {
    std::fprintf(stderr, "VGUI2 native PasteFrame %s failed (%d)\n", operation, error);
}

[[noreturn]] void FatalNativeFailure(const char* operation, int error) noexcept {
    ReportNativeFailure(operation, error);
    std::fflush(stderr);
    std::abort();
}

class RegistryLock {
public:
    RegistryLock() noexcept : error_(pthread_mutex_lock(&nativeRegistry.mutex)), locked_(error_ == 0) {
    }
    ~RegistryLock() noexcept {
        Unlock();
    }
    RegistryLock(const RegistryLock&) = delete;
    RegistryLock& operator=(const RegistryLock&) = delete;

    bool Locked() const noexcept {
        return locked_;
    }
    int Error() const noexcept {
        return error_;
    }
    void Unlock() noexcept {
        if (locked_) {
            const int error = pthread_mutex_unlock(&nativeRegistry.mutex);
            if (error != 0) {
                FatalNativeFailure("mutex_unlock", error);
            }
            locked_ = false;
        }
    }

private:
    int error_;
    bool locked_;
};

bool RegistryOpen() noexcept {
    if (nativeRegistry.phase.load(std::memory_order_acquire) != RegistryPhase::Open) {
        std::fputs("VGUI2 native PasteFrame admission is closed\n", stderr);
        return false;
    }
    return true;
}

#else
thread_local PasteFrame* activePasteFrame = nullptr;
#endif

} // namespace

Allocator DefaultAllocator() {
    return {&std::malloc, &std::free};
}

Buffer::Buffer(Allocator allocator) : allocator_(allocator), data_(nullptr), length_(0) {
}

Buffer::~Buffer() {
    if (data_) {
        allocator_.release(data_);
    }
}

bool Buffer::Allocate(int capacity) {
    if (capacity <= 0 || capacity > MaximumCapacity || !allocator_.allocate || !allocator_.release) {
        return false;
    }
    const std::size_t cells = static_cast<std::size_t>(capacity) + 1;
    if (cells > std::numeric_limits<std::size_t>::max() / sizeof(wchar_t)) {
        return false;
    }
    const std::size_t bytes = cells * sizeof(wchar_t);
    data_ = static_cast<wchar_t*>(allocator_.allocate(bytes));
    if (!data_) {
        return false;
    }
    std::memset(data_, 0, bytes);
    return true;
}

void Buffer::Swap(Buffer& other) {
    std::swap(data_, other.data_);
    std::swap(length_, other.length_);
}

bool Buffer::Read(int initialCapacity, bool requireCompleteText, Reader reader) {
    if (!reader || initialCapacity <= 0 || initialCapacity > MaximumCapacity) {
        return false;
    }
    int capacity = initialCapacity;
    for (;;) {
        Buffer candidate(allocator_);
        if (!candidate.Allocate(capacity)) {
            return false;
        }
        int count = reader(candidate.data_, capacity * static_cast<int>(sizeof(wchar_t)));
        if (count <= 0) {
            return false;
        }
        if (count > capacity) {
            count = capacity;
        }
        int logicalLength = 0;
        while (logicalLength < count && candidate.data_[logicalLength] != 0) {
            ++logicalLength;
        }
        if (logicalLength < count) {
            candidate.length_ = logicalLength;
            Swap(candidate);
            return true;
        }
        if (count < capacity) {
            return false; // A short unterminated span is not a complete text envelope.
        }
        if (!requireCompleteText) {
            candidate.length_ = capacity - 1;
            candidate.data_[candidate.length_] = 0;
            Swap(candidate);
            return true;
        }
        if (capacity == MaximumCapacity) {
            return false;
        }
        capacity = capacity > MaximumCapacity / 2 ? MaximumCapacity : capacity * 2;
    }
}

bool Buffer::CopyExpanded(const wchar_t* source, int sourceLength) {
    if (sourceLength < 0 || sourceLength > MaximumCapacity || (!source && sourceLength != 0)) {
        return false;
    }
    int expandedLength = sourceLength;
    for (int index = 0; index < sourceLength; ++index) {
        if (source[index] == L'\n') {
            if (expandedLength == MaximumCapacity) {
                return false;
            }
            ++expandedLength;
        }
    }
    Buffer candidate(allocator_);
    if (!candidate.Allocate(expandedLength > 0 ? expandedLength : 1)) {
        return false;
    }
    int output = 0;
    for (int index = 0; index < sourceLength; ++index) {
        if (source[index] == L'\n') {
            candidate.data_[output++] = L'\r';
        }
        candidate.data_[output++] = source[index];
    }
    candidate.length_ = expandedLength;
    Swap(candidate);
    return true;
}

const wchar_t* Buffer::Data() const {
    return data_;
}

int Buffer::Length() const {
    return length_;
}

#if defined(__APPLE__) && !defined(_WIN32) && defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) \
    && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1

PasteFrame::PasteFrame(const void* panel, int logicalRemainder)
    : previous_(nullptr), panel_(panel), logicalRemainder_(logicalRemainder), entered_(false) {
    if (!panel || logicalRemainder < 0 || !RegistryOpen()) {
        return;
    }
    RegistryLock lock;
    if (!lock.Locked()) {
        ReportNativeFailure("mutex_lock", lock.Error());
        return;
    }
    if (!RegistryOpen()) {
        return;
    }
    if (!nativeRegistry.keyCreated) {
        const int error = pthread_key_create(&nativeRegistry.key, nullptr);
        if (error != 0) {
            ReportNativeFailure("key_create", error);
            return;
        }
        nativeRegistry.keyCreated = true;
    }
    previous_ = static_cast<PasteFrame*>(pthread_getspecific(nativeRegistry.key));
    for (const PasteFrame* frame = previous_; frame; frame = frame->previous_) {
        if (frame->panel_ == panel && logicalRemainder >= frame->logicalRemainder_) {
            return;
        }
    }
    if (nativeRegistry.activeFrames == std::numeric_limits<std::uint64_t>::max()) {
        std::fputs("VGUI2 native PasteFrame count exhausted\n", stderr);
        return;
    }
    const int error = pthread_setspecific(nativeRegistry.key, this);
    if (error != 0) {
        if (pthread_getspecific(nativeRegistry.key) != previous_) {
            FatalNativeFailure("head changed after failed setspecific", error);
        }
        ReportNativeFailure("setspecific", error);
        return;
    }
    ++nativeRegistry.activeFrames;
    entered_ = true;
}

PasteFrame::~PasteFrame() {
    if (!entered_) {
        return;
    }
    RegistryLock lock;
    if (!lock.Locked()) {
        FatalNativeFailure("destructor mutex_lock", lock.Error());
    }
    if (!nativeRegistry.keyCreated || nativeRegistry.activeFrames == 0
        || pthread_getspecific(nativeRegistry.key) != this) {
        FatalNativeFailure("invalid live frame head", 0);
    }
    const int error = pthread_setspecific(nativeRegistry.key, previous_);
    if (error != 0) {
        FatalNativeFailure("destructor setspecific", error);
    }
    --nativeRegistry.activeFrames;
}

const void* NativePasteFrameIdentity() noexcept {
    return &nativeRegistry;
}

NativeFrameRetireResult RetireNativePasteFrames(const NativeFrameQuiescence& proof) noexcept {
    if (proof.structBytes != sizeof(proof) || proof.version != QuiescenceVersion || !proof.context
        || proof.moduleIdentity != NativePasteFrameIdentity() || !proof.isHeld) {
        return NativeFrameRetireResult::InvalidProof;
    }
    if (!proof.isHeld(proof.context, proof.moduleIdentity)) {
        return NativeFrameRetireResult::QuiescenceRequired;
    }
    auto phase = nativeRegistry.phase.load(std::memory_order_acquire);
    if (phase == RegistryPhase::Retired) {
        return NativeFrameRetireResult::Retired;
    }
    if (phase != RegistryPhase::MutexRetirementPending) {
        RegistryLock lock;
        if (!lock.Locked()) {
            ReportNativeFailure("retirement mutex_lock", lock.Error());
            return NativeFrameRetireResult::MutexLockFailed;
        }
        if (nativeRegistry.activeFrames != 0) {
            return NativeFrameRetireResult::Busy;
        }
        nativeRegistry.phase.store(RegistryPhase::KeyRetirementPending, std::memory_order_release);
        if (nativeRegistry.keyCreated) {
            const int error = pthread_key_delete(nativeRegistry.key);
            if (error != 0) {
                ReportNativeFailure("key_delete", error);
                return NativeFrameRetireResult::KeyDeleteFailed;
            }
            nativeRegistry.keyCreated = false;
        }
        nativeRegistry.phase.store(RegistryPhase::MutexRetirementPending, std::memory_order_release);
        lock.Unlock();
    }
    // The borrowed exclusive host proof excludes every call/object/worker and
    // blocked registry operation. A frame count of zero cannot establish that.
    const int error = pthread_mutex_destroy(&nativeRegistry.mutex);
    if (error != 0) {
        ReportNativeFailure("mutex_destroy", error);
        return NativeFrameRetireResult::MutexDestroyFailed;
    }
    nativeRegistry.phase.store(RegistryPhase::Retired, std::memory_order_release);
    return NativeFrameRetireResult::Retired;
}

#else

PasteFrame::PasteFrame(const void* panel, int logicalRemainder)
    : previous_(activePasteFrame), panel_(panel), logicalRemainder_(logicalRemainder), entered_(false) {
    if (!panel || logicalRemainder < 0) {
        return;
    }
    for (const PasteFrame* frame = activePasteFrame; frame; frame = frame->previous_) {
        if (frame->panel_ == panel && logicalRemainder >= frame->logicalRemainder_) {
            return;
        }
    }
    activePasteFrame = this;
    entered_ = true;
}

PasteFrame::~PasteFrame() {
    if (entered_) {
        activePasteFrame = previous_;
    }
}

#endif

bool PasteFrame::Entered() const {
    return entered_;
}

} // namespace cse_textentry_clipboard
