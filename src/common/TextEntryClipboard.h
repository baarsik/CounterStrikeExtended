#ifndef CSE_TEXT_ENTRY_CLIPBOARD_H
#define CSE_TEXT_ENTRY_CLIPBOARD_H

#include <cstddef>

// Private, unselected Apple candidate. Root must supply checked whole-module
// lifetime/retirement hooks before enabling it in any production owner.
#if defined(__APPLE__) && !defined(_WIN32) && defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY)
#if CSE_TEXT_ENTRY_USE_PTHREAD_KEY != 0 && CSE_TEXT_ENTRY_USE_PTHREAD_KEY != 1
#error CSE_TEXT_ENTRY_USE_PTHREAD_KEY must be zero or one
#endif
#if CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1
#include <cstdint>
#endif
#endif

#if defined(_WIN32)
#define CSE_TEXT_ENTRY_PRIVATE
#elif defined(__GNUC__) || defined(__clang__)
#define CSE_TEXT_ENTRY_PRIVATE __attribute__((visibility("hidden")))
#else
#error The private text caller helper requires noninterposable image-local binding
#endif

namespace cse_textentry_clipboard {

// Private caller storage. The reader writes at most byteCapacity bytes and
// returns copied wchar cells. Allocator callbacks own no state and must not throw.
struct Allocator {
    void* (*allocate)(std::size_t bytes);
    void (*release)(void* memory);
};

CSE_TEXT_ENTRY_PRIVATE Allocator DefaultAllocator();

class CSE_TEXT_ENTRY_PRIVATE Buffer {
public:
    explicit Buffer(Allocator allocator = DefaultAllocator());
    ~Buffer();

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    using Reader = int (*)(wchar_t* destination, int byteCapacity);

    // Failed operations leave the previous buffer intact. Read stages a whole
    // snapshot before mutation; independent rereads cannot promise atomicity.
    bool Read(int initialCapacity, bool requireCompleteText, Reader reader);
    bool CopyExpanded(const wchar_t* source, int sourceLength);
    const wchar_t* Data() const;
    int Length() const;

private:
    bool Allocate(int capacity);
    void Swap(Buffer& other);

    Allocator allocator_;
    wchar_t* data_;
    int length_;
};

// Synchronous, ephemeral per-thread/per-module recursion state. It neither owns
// a widget nor makes callbacks safe if external code deletes a live widget.
class CSE_TEXT_ENTRY_PRIVATE PasteFrame {
public:
    PasteFrame(const void* panel, int logicalRemainder);
    ~PasteFrame();

    PasteFrame(const PasteFrame&) = delete;
    PasteFrame& operator=(const PasteFrame&) = delete;

    bool Entered() const;

private:
    PasteFrame* previous_;
    const void* panel_;
    int logicalRemainder_;
    bool entered_;
};

#if defined(__APPLE__) && !defined(_WIN32) && defined(CSE_TEXT_ENTRY_USE_PTHREAD_KEY) \
    && CSE_TEXT_ENTRY_USE_PTHREAD_KEY == 1

enum class NativeFrameRetireResult : std::uint32_t {
    Retired,
    InvalidProof,
    QuiescenceRequired,
    Busy,
    MutexLockFailed,
    KeyDeleteFailed,
    MutexDestroyFailed
};

// Borrowed witness, NOT intrinsic loader safety. The real host must hold genuine
// image/context/code references, close whole-module admission and drain complete
// calls, objects and workers, not just entered PasteFrames. It keeps that exclusive
// quiescence through retirement return and releases final refs outside module code.
// isHeld is nonthrowing/nonreentrant and checks this exact live module identity.
// It cannot turn a counter or dishonest callback into proof. Nothing is retained.
struct NativeFrameQuiescence {
    std::uint32_t structBytes;
    std::uint32_t version;
    void* context;
    const void* moduleIdentity;
    bool (*isHeld)(void*, const void*);
};

CSE_TEXT_ENTRY_PRIVATE const void* NativePasteFrameIdentity() noexcept;
// No automatic reopen, TSD destructor, atexit hook or late callback. Partial
// failures retain/quarantine resources and forbid successful unload/rebind.
// Calls/lifecycle must be externally serialized around retirement. A successful
// result retires this registry only; it does not qualify the entire image unload.
CSE_TEXT_ENTRY_PRIVATE NativeFrameRetireResult RetireNativePasteFrames(const NativeFrameQuiescence&) noexcept;

#endif

} // namespace cse_textentry_clipboard

#undef CSE_TEXT_ENTRY_PRIVATE

#endif
