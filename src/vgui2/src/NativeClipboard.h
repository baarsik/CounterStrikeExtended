#ifndef CSE_VGUI2_NATIVE_CLIPBOARD_H
#define CSE_VGUI2_NATIVE_CLIPBOARD_H

#include <cstdint>
#include <type_traits>

namespace cse_vgui2_clipboard {

enum class Status : std::int32_t {
    Ok,
    MissingFormat,
    InvalidArgument,
    InvalidSpan,
    OutOfRange,
    MalformedUnicode,
    InvalidScalar,
    InsufficientStorage,
    ResultOutOfRange,
    OverlappingStorage
};

struct ByteSpan {
    const unsigned char* data;
    std::uint64_t storageBytes;
};

struct ByteBuffer {
    unsigned char* data;
    std::uint64_t storageBytes;
};

struct WideSpan {
    const wchar_t* data;
    std::uint64_t storageBytes;
};

struct WideBuffer {
    wchar_t* data;
    std::uint64_t storageBytes;
};

// Borrowed, immutable views for one pure call. The future owner must retain
// storage/context/code and enforce thread/shutdown rules; this is not a lease.
// Count is independent of Unicode. The raw allocation may include odd padding;
// wide reads use an explicit even unit extent and preserve NUL-containing tails.
struct SnapshotView {
    std::uint64_t generation;
    bool hasLegacyTextCount;
    std::uint64_t legacyTextStorageBytes;
    bool hasUnicode;
    ByteSpan unicodeStorage;
    std::uint64_t unicodeUnitExtentBytes;
};

static_assert(std::is_trivial<SnapshotView>::value && std::is_standard_layout<SnapshotView>::value,
    "Clipboard snapshots are borrowed POD views, not an owner or service registry");

// All failures leave destination AND out-count unchanged. On success, counts
// describe actual complete units/bytes, including any copied NUL/tail storage.
// Supplied spans must cover real readable/writable storage; inputs/metadata
// must remain immutable for the whole call. Relevant input,
// output and out-count storage may not overlap; span operations reject overlap.
// No read trims at NUL or forces a destination terminator. No function allocates,
// publishes, selects an external encoding, or accesses a clipboard/provider.
Status CountLegacyText(const SnapshotView& snapshot, std::int32_t& outCount) noexcept;
Status ReadUnicodeBytes(const SnapshotView& snapshot, std::int32_t byteOffset, ByteBuffer destination,
    std::int32_t& outBytes) noexcept;
Status ReadUnicodeCells(const SnapshotView& snapshot, std::int32_t unitOffset, WideBuffer destination,
    std::int32_t& outCells) noexcept;

// Length-only byte planning does not inspect source storage. Packing checks its
// exact explicit-length prefix and appends one zero byte/unit. Wide input keeps
// raw 0..FFFF units, expands valid supplementary native32 scalars, and rejects
// invalid native32 values before writing. Embedded NUL does not end packing.
Status PlanLegacyBytes(std::int32_t textLength, std::uint64_t& outStorageBytes) noexcept;
Status PackLegacyBytes(ByteSpan source, std::int32_t textLength, ByteBuffer destination,
    std::uint64_t& outStorageBytes) noexcept;
Status PlanWideCells(WideSpan source, std::int32_t textLength, std::uint64_t& outStorageBytes) noexcept;
Status PackWideCells(WideSpan source, std::int32_t textLength, ByteBuffer destination,
    std::uint64_t& outStorageBytes) noexcept;

} // namespace cse_vgui2_clipboard

#endif
